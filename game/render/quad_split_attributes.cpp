#include "render/quad_split_attributes.h"

#include "render/split_scratch.h"

#include <array>
#include <optional>

namespace ts2 {
namespace {

// The values a child's corner can take: a parent corner, the middle of two corners, or the centre.
enum Source : unsigned { K0, K1, K2, K3, M01, M02, M13, M23, Centre, kSources };
using Values = std::array<std::uint32_t, kSources>;
using ChildSources = std::array<std::array<Source, 4>, 4>;

constexpr ChildSources kQuarters{{
    {K0, M01, M02, Centre},
    {M01, K1, Centre, M13},
    {M02, Centre, K2, M23},
    {Centre, M13, M23, K3},
}};
constexpr ChildSources kFan{{
    {K0, Centre, M01, K1},
    {K1, Centre, M13, K3},
    {K2, Centre, M02, K0},
    {K3, Centre, M23, K2},
}};

Values colourValues(const MeshCpu &c, const SplitFormat &f, std::uint32_t parent) {
  const auto corner = [&](unsigned i) {
    return c.lw(parent + f.colour(i)) & 0x00FEFEFEu;
  };
  const std::array<std::uint32_t, 4> k{corner(0), corner(1), corner(2), corner(3)};
  return {k[0],
          k[1],
          k[2],
          k[3],
          (k[0] + k[1]) >> 1,
          (k[0] + k[2]) >> 1,
          (k[3] + k[1]) >> 1,
          (k[3] + k[2]) >> 1,
          (k[2] + k[1]) >> 1};
}

// The uv values of the nine sources, and the scratchpad carries the guest keeps while it averages.
struct UvSplit {
  Values values;
  std::uint32_t high0, high1;
  std::array<std::uint32_t, 5> carries; // 01, 13, 02, 23, 12
};

UvSplit uvValues(const MeshCpu &c, const SplitFormat &f, std::uint32_t parent) {
  const std::uint32_t w0 = c.lw(parent + f.uv(0));
  const std::uint32_t w1 = c.lw(parent + f.uv(1));
  const std::uint32_t u2 = c.lhu(parent + f.uv(2));
  const std::uint32_t u3 = c.lhu(parent + f.uv(3));
  const std::uint32_t carry01 = (w0 & 0x101u) & w1;
  const std::uint32_t carry13 = (w1 & 0x101u) & u3;
  const std::uint32_t carry02 = (w0 & 0x101u) & u2;
  const std::uint32_t carry23 = (u2 & 0x101u) & u3;
  const std::uint32_t carry12 = (w1 & 0x101u) & u2;
  const std::uint32_t k0 = w0 & 0xFFFFu;
  const std::uint32_t k1 = w1 & 0xFFFFu;
  return {{k0,
           k1,
           u2,
           u3,
           meanUv(k0, k1, carry01),
           meanUv(k0, u2, carry02),
           meanUv(k1, u3, carry13),
           meanUv(u2, u3, carry23),
           meanUv(k1, u2, carry12)},
          (w0 >> 16) << 16,
          (w1 >> 16) << 16,
          {carry01, carry13, carry02, carry23, carry12}};
}

// The guest keeps four of the five carries in the scratchpad: all but 01 for quarters, all but 12 for the fan of
// fresh children, and none for an adopted fan, which holds them in registers.
void parkSplitCarries(const MeshCpu &c, SplitLayout layout, SplitOrigin origin, const std::array<std::uint32_t, 5> &k) {
  if (layout == SplitLayout::Quarters) {
    for (unsigned i = 0; i != 4; ++i) {
      c.sh(kSplitCarries + 4u + 4u * i, k[i + 1u]);
    }
  } else if (origin == SplitOrigin::Fresh) {
    for (unsigned i = 0; i != 4; ++i) {
      c.sh(kSplitCarries + 4u * i, k[i]);
    }
  }
}

} // namespace

void splitQuadAttributes(MeshCpu &c,
                         const SplitFormat &format,
                         SplitLayout layout,
                         SplitOrigin origin,
                         std::uint32_t parent,
                         std::span<const std::uint32_t> primary,
                         std::span<const std::uint32_t> alt) {
  const ChildSources &sources = layout == SplitLayout::Quarters ? kQuarters : kFan;
  if (layout == SplitLayout::Quarters || origin == SplitOrigin::Fresh) {
    c.sw(kSplitSavedT8, c.t8);
  }
  const Values colours = colourValues(c, format, parent);
  const std::uint32_t code = (c.lw(parent + format.colour(0)) >> 24) << 24;
  std::optional<UvSplit> uvs;
  if (format.textured()) {
    uvs = uvValues(c, format, parent);
    parkSplitCarries(c, layout, origin, uvs->carries);
  }
  for (const auto children : {primary, alt}) {
    for (std::size_t child = 0; child != children.size(); ++child) {
      const std::uint32_t at = children[child];
      const auto &s = sources[child];
      for (unsigned slot = 0; slot != 4; ++slot) {
        c.sw(at + format.colour(slot), colours[s[slot]] + (slot == 0 ? code : 0u));
      }
      if (uvs) {
        c.sw(at + format.uv(0), uvs->values[s[0]] + uvs->high0);
        c.sw(at + format.uv(1), uvs->values[s[1]] + uvs->high1);
        c.sh(at + format.uv(2), uvs->values[s[2]]);
        c.sh(at + format.uv(3), uvs->values[s[3]]);
      }
    }
  }
  // The guest's last masked operand and last mean stay in v1 and at, which a split with every child culled
  // returns.
  if (uvs) {
    c.v1 = uvs->values[K2] & 0xFEFEu;
    c.at = layout == SplitLayout::Fan ? uvs->values[M23] : uvs->values[Centre] + uvs->high1;
  } else {
    c.v1 = colours[K3];
    c.at = colours[M13];
  }
}

} // namespace ts2
