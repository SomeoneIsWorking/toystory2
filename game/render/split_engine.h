// The part of the near-camera quad and triangle subdividers' split path that does not depend on the shape:
// the frame at t9, the free-list allocation of children, the culling, recursion and linking of a child, and the
// fan of edge children.
#pragma once

#include "render/mesh_cpu.h"
#include "render/split_format.h"
#include "render/split_layout.h"

#include <array>
#include <span>

namespace ts2 {

// The frame the split keeps at t9, 144 bytes per recursion level. The parent's vertex addresses come first.
namespace split_frame {
inline constexpr std::uint32_t kVertices = 0;
inline constexpr std::uint32_t kSaved = 16; // a0, a1, s0, s2, s5, s6 as the caller left them
inline constexpr std::uint32_t kNear = 40;  // a1 and s4, as halfwords
inline constexpr std::uint32_t kMask = 100; // which tested points lie on screen, x byte over y byte
inline constexpr std::uint32_t kSize = 144;
} // namespace split_frame

// Where the frame keeps one of the split's points: its corners and the middles of its edges and diagonals.
struct PointSlot {
  std::uint32_t depth; // SZ, a halfword for a corner and a word for a middle
  std::uint32_t sxy;
  std::uint32_t vertex; // a middle's vertex record in the frame; a corner's address is the frame's input
  bool corner;
};

// A child of the fan, on one edge: the depths summed to place it, the corners of its quad form (the edge's
// middle is the third) and of its triangle form, the point mask, and the short-edge bit that says which form.
struct Band {
  std::array<unsigned, 4> depths;
  std::array<unsigned, 4> quad;
  std::array<unsigned, 3> triangle;
  std::uint32_t mask;
  std::uint32_t edge;
};

class SplitEngine {
protected:
  SplitEngine(
      MeshCpu &c, const SplitFormat &format, std::span<const PointSlot> points, unsigned corners, unsigned fanBelow)
      : c_(c), f_(format), points_(points), corners_(corners), fanBelow_(fanBelow) {}
  ~SplitEngine() = default;

  // Writes the children's attributes: `primary` are their packets in the array drawn from, `alt` in the other
  // (empty for adopted children).
  virtual void splitAttributes(SplitLayout layout,
                               SplitOrigin origin,
                               std::uint32_t parent,
                               std::span<const std::uint32_t> primary,
                               std::span<const std::uint32_t> alt) = 0;

  std::uint32_t &child(unsigned i);
  void saveFrame();
  void leave();
  void release(std::uint32_t site);

  // Gives the primitive its children: new ones off the free list, or those a past split left. False when the
  // free list has none to give.
  bool settleChildren();
  bool fan() const;

  void storeScreenMask(std::uint32_t flags,
                       std::uint32_t xCentre,
                       std::uint32_t yCentre,
                       std::span<const std::uint32_t> points);
  bool visible(std::uint32_t mask) const;
  std::uint32_t depth(unsigned point) const;
  std::uint32_t vertexOf(unsigned point) const;

  // Quarters.
  void cullQuarter(unsigned i);
  bool subdivideChild(unsigned i, std::span<const unsigned> ring);
  void releaseWhole(unsigned i);
  void link(std::uint32_t packet, std::uint32_t bucket, std::uint32_t command, std::span<const unsigned> corners);

  // Fan.
  void releaseSpare();
  void band(unsigned i, const Band &band);

  MeshCpu &c_;
  const SplitFormat &f_;

private:
  void adoptChildren();
  void swapLastCorners(std::uint32_t packet);

  std::span<const PointSlot> points_;
  unsigned corners_;
  unsigned fanBelow_;
  bool fan_ = false;
};

} // namespace ts2
