#include "render/slot_mesh_producers.h"

#include "core.h"
#include "execution_exit.h"
#include "native_dispatch.h"
#include "render/mesh_format.h"

#include <optional>

namespace ts2 {
namespace {

std::uint32_t entriesPerPrimitive(std::uint32_t submitter, std::uint8_t opcode) {
  const bool doublePacket = submitter == SlotMeshProducers::kStaticMeshSubmitter && opcode >= 8u && opcode < 16u;
  return doublePacket ? 2u : 1u;
}

void keyFaces(Core &core, std::uint32_t submitter, std::uint32_t mesh, std::uint32_t table, std::uint32_t packets) {
  const std::uint32_t entries = SlotMeshProducers::slotEntries(core, submitter, mesh);
  for (std::uint32_t entry = 0; entry != entries; ++entry) {
    const std::uint32_t slot = core.mem_r16(table + entry * 2u);
    if (slot == 0u) {
      continue;
    }
    const psx::present::EmissionScope::Guard face(core.emission, submitter, table, entry);
    core.emission.bindPacket(packets + slot * 4u);
  }
}

template <std::uint32_t Submitter> void submit(Core *core) {
  const std::uint32_t mesh = core->r[4];
  const std::uint32_t table = core->mem_r32(SlotMeshProducers::kInstanceSlotTable);
  const std::uint32_t packets = core->mem_r32(SlotMeshProducers::kSlotPacketBase);
  psx::cpu::callOriginalToReturn(*core, Submitter, psx::cpu::ExecutionBudget::currentTurn(*core), "slot mesh producer");
  keyFaces(*core, Submitter, mesh, table, packets);
}

} // namespace

// No dispatcher scope: it would key the body's unnamed pool packets by mesh, which pairs them by order.
void SlotMeshProducers::install(Core &core) {
  psx::cpu::installNativeOverride(core, kStaticMeshSubmitter, "static-mesh-producer", &submit<kStaticMeshSubmitter>);
  psx::cpu::installNativeOverride(core, kRigidMeshDrawer, "rigid-mesh-producer", &submit<kRigidMeshDrawer>);
}

std::uint32_t SlotMeshProducers::slotEntries(Core &core, std::uint32_t submitter, std::uint32_t mesh) {
  const std::optional<ResidentMeshLayout> layout = decodeResidentMeshLayout(core, mesh);
  // 0x80017FF8 reads only the positive vertex-count header.
  if (!layout || (submitter == kRigidMeshDrawer && layout->hasAuxiliaryVertexRecords)) {
    return 0u;
  }
  std::uint32_t entries = 0;
  std::uint32_t address = layout->commandAddress;
  while (true) {
    const std::optional<ResidentMeshCommand> command = decodeResidentMeshCommand(core, address);
    if (!command) {
      return 0u;
    }
    if (command->terminal) {
      return entries;
    }
    entries += command->primitiveCount * entriesPerPrimitive(submitter, command->opcode);
    address = command->nextCommandAddress;
  }
}

} // namespace ts2
