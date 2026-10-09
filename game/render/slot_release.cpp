#include "render/slot_release.h"

#include <array>

namespace ts2 {
namespace {

constexpr std::uint32_t kStackTop = kMeshScratch + 0x360u; // the software stack the submitters keep in the scratchpad
constexpr std::uint32_t kFlag = 39u;
constexpr std::uint32_t kFirstIds = 8u;

// The return addresses the guest's own calls leave in ra: the wrapper's call into the node routine, then each
// recursive call from it.
struct Sites {
  std::uint32_t entry;
  std::array<std::uint32_t, 4> child;
};
constexpr Sites kTexturedSites{0x80017B58u, {0x80017BF0u, 0x80017C0Cu, 0x80017C28u, 0x80017C44u}};
constexpr Sites kPlainSites{0x80017EF0u, {0x80017F88u, 0x80017FA4u, 0x80017FC0u, 0x80017FDCu}};

// 0x80017B6C and 0x80017F04: one packet and, recursively, every flagged child below it. The caller's t0..t3 and
// ra are pushed on the scratchpad stack; the other array's packet lies at the same offset from its base (v0), and
// a negative flag says the ids to follow are in that other copy.
void releaseNode(MeshCpu &c, std::uint32_t secondIds, const Sites &sites) {
  c.sw(c.at - 4u, c.t0);
  c.sw(c.at - 8u, c.t1);
  c.sw(c.at - 12u, c.t2);
  c.sw(c.at - 16u, c.t3);
  c.sw(c.at - 20u, c.ra);
  c.at -= 20u;
  c.t0 = c.lb(c.a3 + kFlag);
  c.t1 = c.a3 - c.s3;
  c.t4 = c.t1 + c.v0;
  if (negative(c.t0)) {
    c.a3 = c.t1 + c.v0;
    c.t4 = c.t1 + c.s3;
  }
  c.t0 = c.lw(c.a3 + kFirstIds);
  c.t2 = c.lw(c.a3 + secondIds);
  c.t1 = c.t0 >> 16;
  c.t0 &= 0x7FFFu;
  c.t3 = c.t2 >> 16;
  c.t2 &= 0x7FFFu;
  c.sh(c.v1, c.t0);
  c.sh(c.v1 + 2u, c.t1);
  c.sh(c.v1 + 4u, c.t2);
  c.sh(c.v1 + 6u, c.t3);
  c.v1 += 8u;
  c.sb(c.a3 + kFlag, 0);
  c.sb(c.t4 + kFlag, 0);
  const std::array<std::uint32_t, 4> ids{c.t0, c.t1, c.t2, c.t3};
  c.t0 <<= 2;
  for (std::size_t i = 0; i != ids.size(); ++i) {
    c.a3 = c.t0 + c.s3;
    c.t4 = c.lb(c.a3 + kFlag);
    c.t0 = ids[i + 1u < ids.size() ? i + 1u : i] << 2;
    if (c.t4 != 0u) {
      c.ra = sites.child[i];
      releaseNode(c, secondIds, sites);
    }
  }
  c.at += 20u;
  c.ra = c.lw(c.at - 20u);
  c.t0 = c.lw(c.at - 4u);
  c.t1 = c.lw(c.at - 8u);
  c.t2 = c.lw(c.at - 12u);
  c.t3 = c.lw(c.at - 16u);
}

} // namespace

void releaseSubtree(MeshCpu &c, PacketKind kind) {
  const bool textured = kind == PacketKind::Textured;
  const Sites &sites = textured ? kTexturedSites : kPlainSites;
  c.at = kStackTop;
  c.sw(c.at - 4u, c.t4);
  c.sw(c.at - 8u, c.ra);
  c.v0 = c.lw(c.at - 792u);
  c.v1 = c.lw(c.at - 788u);
  c.at -= 8u;
  c.ra = sites.entry;
  releaseNode(c, textured ? 20u : 16u, sites);
  c.at += 8u;
  c.ra = c.lw(c.at - 8u);
  c.t4 = c.lw(c.at - 4u);
  c.sw(c.at - 788u, c.v1);
}

} // namespace ts2
