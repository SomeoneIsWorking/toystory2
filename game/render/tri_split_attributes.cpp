#include "render/tri_split_attributes.h"

#include "render/split_scratch.h"

#include <algorithm>
#include <array>
#include <optional>

namespace ts2 {
namespace {

// The words of one child, by corner slot; a fan child has a fourth.
using ChildWords = std::array<std::uint32_t, 4>;
using Children = std::array<ChildWords, 4>;

// Colours: the corners masked of their low bits so that sums do not carry between bytes, and the command byte
// the first slot keeps. A fan's children may be quads, so its command byte gains the quad bit.
Children colourWords(const MeshCpu &c, const SplitFormat &f, SplitLayout layout, std::uint32_t parent) {
  const auto corner = [&](unsigned i) {
    return c.lw(parent + f.colour(i)) & 0x00FEFEFEu;
  };
  const std::uint32_t c0 = corner(0);
  const std::uint32_t c1 = corner(1);
  const std::uint32_t c2 = corner(2);
  const std::uint32_t command = c.lw(parent + f.colour(0)) >> 24;
  const std::uint32_t m01 = (c0 + c1) >> 1;
  const std::uint32_t m02 = (c0 + c2) >> 1;
  const std::uint32_t m12 = (c2 + c1) >> 1;
  if (layout == SplitLayout::Quarters) {
    const std::uint32_t code = command << 24;
    return {{{c0 + code, m01, m02, 0}, {m01 + code, c1, m12, 0}, {m02 + code, m12, c2, 0}, {m01 + code, m12, m02, 0}}};
  }
  const std::uint32_t code = (command | 8u) << 24;
  const std::uint32_t centre = ((m01 & 0x00FEFEFEu) + c2) >> 1;
  return {{{c0 + code, centre, m01, c1}, {c1 + code, centre, m12, c2}, {c2 + code, centre, m02, c0}, {}}};
}

// The carries the uv means add back: of the first two corners, the first and third, and the third and second.
struct UvCarries {
  std::uint32_t k01, k02, k21;
};

struct UvWords {
  Children children;
  UvCarries carries;
  std::uint32_t last;  // the guest's last mean, which stays in at
  std::uint32_t third; // the third corner's uv, which v1 keeps masked
};

// A fan's centre ORs in the third corner's low bits whatever the means carry, and its second and third children
// take each other's carries: the guest's own arithmetic, kept as it is.
UvWords uvWords(const MeshCpu &c, const SplitFormat &f, SplitLayout layout, std::uint32_t parent) {
  const std::uint32_t w0 = c.lw(parent + f.uv(0));
  const std::uint32_t w1 = c.lw(parent + f.uv(1));
  const std::uint32_t u2 = c.lhu(parent + f.uv(2));
  const std::uint32_t high0 = (w0 >> 16) << 16;
  const std::uint32_t high1 = (w1 >> 16) << 16;
  const std::uint32_t k0 = w0 & 0xFFFFu;
  const std::uint32_t k1 = w1 & 0xFFFFu;
  const UvCarries carries{(w0 & 0x101u) & w1, (w0 & 0x101u) & u2, (u2 & 0x101u) & w1};
  const std::uint32_t m01 = meanUv(k0, k1, carries.k01);
  if (layout == SplitLayout::Quarters) {
    const std::uint32_t m02 = meanUv(k0, u2, carries.k02);
    const std::uint32_t m12 = meanUv(u2, k1, carries.k21);
    return {{{{k0 + high0, m01 + high1, m02, 0},
              {m01 + high0, k1 + high1, m12, 0},
              {m02 + high0, m12 + high1, u2, 0},
              {m01 + high0, m12 + high1, m02, 0}}},
            carries,
            m01 + high1,
            u2};
  }
  const std::uint32_t low = high1 + (u2 & 0x101u);
  const std::uint32_t centre = ((((m01 & 0xFEFEu) + (u2 & 0xFEFEu)) >> 1) & 0xFEFEu) | low | (low & m01);
  const std::uint32_t second = meanUv(u2, k1, carries.k02);
  const std::uint32_t third = meanUv(u2, k0, carries.k21);
  return {{{{k0 + high0, centre, m01, k1}, {k1 + high0, centre, second, u2}, {u2 + high0, centre, third, k0}, {}}},
          carries,
          second,
          u2};
}

// Where the guest parks the carries while it works: the quarters keep two, the fan three.
void parkCarries(const MeshCpu &c, SplitLayout layout, const UvCarries &k) {
  if (layout == SplitLayout::Quarters) {
    c.sh(kSplitCarries + 4u, k.k21);
    c.sh(kSplitCarries + 8u, k.k02);
    return;
  }
  c.sh(kSplitCarries, k.k01);
  c.sh(kSplitCarries + 4u, k.k02);
  c.sh(kSplitCarries + 8u, k.k21);
}

} // namespace

void splitTriangleAttributes(MeshCpu &c,
                             const SplitFormat &format,
                             SplitLayout layout,
                             std::uint32_t parent,
                             std::span<const std::uint32_t> primary,
                             std::span<const std::uint32_t> alt) {
  const bool fan = layout == SplitLayout::Fan;
  const unsigned slots = fan ? 4u : 3u;
  const std::size_t count = fan ? 3u : 4u;
  c.sw(kSplitSavedT8, c.t8);
  const Children colours = colourWords(c, format, layout, parent);
  std::optional<UvWords> uvs;
  if (format.textured()) {
    uvs = uvWords(c, format, layout, parent);
    parkCarries(c, layout, uvs->carries);
  }
  for (const auto children : {primary, alt}) {
    for (std::size_t child = 0; child != std::min(count, children.size()); ++child) {
      const std::uint32_t at = children[child];
      for (unsigned slot = 0; slot != slots; ++slot) {
        c.sw(at + format.colour(slot), colours[child][slot]);
      }
      if (!uvs) {
        continue;
      }
      c.sw(at + format.uv(0), uvs->children[child][0]);
      c.sw(at + format.uv(1), uvs->children[child][1]);
      c.sh(at + format.uv(2), uvs->children[child][2]);
      if (fan) {
        c.sh(at + format.uv(3), uvs->children[child][3]);
      }
    }
  }
  // The guest's last operands stay in v1 and at, which a split with every child culled returns.
  if (uvs) {
    c.v1 = uvs->third & 0xFEFEu;
    c.at = uvs->last;
  } else {
    c.at = colours[1][2];
  }
}

} // namespace ts2
