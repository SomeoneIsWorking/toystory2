#include "render/slot_mesh_producers.h"

#include "core.h"
#include "execution_exit.h"
#include "native_dispatch.h"
#include "render/mesh_draw_state.h"
#include "render/mesh_drawers.h"
#include "render/mesh_format.h"

#include <lucent/log.h>

#include <memory>
#include <optional>

namespace ts2 {
namespace {

constexpr std::uint32_t kScratch = 0x1F800000u;

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

// s0..s6 are spilled to the scratchpad before the body.
void spillSavedRegisters(Core &core) {
  for (std::uint32_t s = 0; s != 7u; ++s) {
    core.mem_w32(kScratch + s * 4u, core.r[16u + s]);
  }
}

// A call whose commands have no native body: the guest original, its faces keyed.
template <std::uint32_t Submitter> void submitOnGuest(Core *core) {
  const std::uint32_t mesh = core->r[4];
  const std::uint32_t table = core->mem_r32(kInstanceSlotTable);
  const std::uint32_t packets = core->mem_r32(kSlotPacketBase);
  psx::cpu::callOriginalToReturn(*core, Submitter, psx::cpu::ExecutionBudget::currentTurn(*core), "slot mesh producer");
  keyFaces(*core, Submitter, mesh, table, packets);
}

template <std::uint32_t Submitter> void submit(Core *core) {
  const MeshDrawCall call{
      Submitter, {core->r[4], core->r[5], core->r[6], core->r[7]}, core->r[3], core->r[31], core->r[22]};
  const psx::present::EmitMemory guest(*core);
  const std::optional<MeshDrawInputs> inputs = MeshDrawers::plan(guest, call);
  if (!inputs) {
    submitOnGuest<Submitter>(core);
    return;
  }
  spillSavedRegisters(*core);
  // One object per call: the instance's slot table, which every face entry of the call indexes.
  const psx::present::EmissionScope::Guard object(core->emission, Submitter, guest.mem_r32(kInstanceSlotTable), 0);
  const MeshDrawRecorder recorder(*core, call, *inputs);
  const std::optional<MeshDrawResult> result = MeshDrawers::run(guest, call);
  recorder.save(*core, *result);
  core->r[2] = result->v0;
  core->r[3] = result->v1;
}

} // namespace

// No dispatcher scope: it would key the body's unnamed pool packets by mesh, which pairs them by order.
void SlotMeshProducers::install(Core &core) {
  psx::cpu::installNativeOverride(core, kStaticMeshSubmitter, "static-mesh-producer", &submit<kStaticMeshSubmitter>);
  psx::cpu::installNativeOverride(core, kRigidMeshDrawer, "rigid-mesh-producer", &submit<kRigidMeshDrawer>);
  core.stateProducers.install(kStaticMeshSubmitter, std::make_unique<MeshStateRender>(core));
  core.stateProducers.install(kRigidMeshDrawer, std::make_unique<MeshStateRender>(core));
}

std::uint32_t SlotMeshProducers::slotEntries(Core &core, std::uint32_t submitter, std::uint32_t mesh) {
  const std::optional<ResidentMeshLayout> layout = decodeResidentMeshLayout(psx::present::EmitMemory(core), mesh);
  // 0x80017FF8 reads only the positive vertex-count header.
  if (!layout || (submitter == kRigidMeshDrawer && layout->hasAuxiliaryVertexRecords)) {
    return 0u;
  }
  std::uint32_t entries = 0;
  std::uint32_t address = layout->commandAddress;
  while (true) {
    const std::optional<ResidentMeshCommand> command =
        decodeResidentMeshCommand(psx::present::EmitMemory(core), address);
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
