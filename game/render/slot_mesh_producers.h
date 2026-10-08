// The resident mesh submitters as producers: each primitive keyed by its instance and its face.
#pragma once

#include <cstdint>

class Core;

namespace ts2 {

// The caller publishes one u16 per face entry at 0x800A11CC before each call: 0 for none, else the index
// of the face's packet in the current buffer's slot array (word index from 0x1F800044).
class SlotMeshProducers {
public:
  inline static constexpr std::uint32_t kStaticMeshSubmitter = 0x800100E4u;
  inline static constexpr std::uint32_t kRigidMeshDrawer = 0x80017FF8u;
  inline static constexpr std::uint32_t kInstanceSlotTable = 0x800A11CCu;
  inline static constexpr std::uint32_t kSlotPacketBase = 0x1F800044u;

  static void install(Core &core);

  static std::uint32_t slotEntries(Core &core, std::uint32_t submitter, std::uint32_t mesh);
};

} // namespace ts2
