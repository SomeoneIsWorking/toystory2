#include "render/view_matrix.h"

#include "core.h"

namespace ts2::render {
namespace {

// Retail `0x8002C848` publishes its view every tick, and these are the addresses it publishes it to.
// The translation block is written twice — raw title units first at `kGuestTranslation`, then the
// GTE's own sub-word units at `kGuestGteTranslation` — and it is the SECOND that the guest projects
// its world with, so that is the one a view must be built from. Reading the raw block instead would
// place the camera thirty-two times further from the world than the guest places it.
constexpr std::uint32_t kGuestGteTranslation = 0x1F800384u;
constexpr std::uint32_t kGuestRotation = 0x1F800394u;

} // namespace

void readResidentView(Core &core, float view[3][3], float translation[3]) {
  // The rotation is the guest's 3x3 in row-major halfwords, at the GTE's 1.3.12 scale, laid down as a
  // 4x4 with three trailing halfwords this view does not use.
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      const std::uint32_t halfword = kGuestRotation + static_cast<std::uint32_t>(row * 3 + column) * 2u;
      view[row][column] =
          static_cast<float>(static_cast<std::int32_t>(static_cast<std::int16_t>(core.mem_r16(halfword))));
    }
  }
  for (int axis = 0; axis < 3; ++axis) {
    translation[axis] = static_cast<float>(
        static_cast<std::int32_t>(core.mem_r32(kGuestGteTranslation + static_cast<std::uint32_t>(axis) * 4u)));
  }
}

} // namespace ts2::render