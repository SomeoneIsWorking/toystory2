#include "render/mesh_draw_state.h"

#include "core.h"
#include "emit_memory.h"
#include "facts/guest_facts.h"
#include "gte_control.h"
#include "host_ordering_table.h"
#include "render/ordering_tables.h"
#include "state_bytes.h"

#include <cstdlib>
#include <lucent/log.h>
#include <optional>

namespace ts2 {
namespace {

using psx::present::EmitMemory;
using psx::present::GteControl;
using psx::present::HostMemory;

struct StateHeader {
  MeshDrawCall call;
  std::uint32_t tableBase = 0; // bucket 0 of the ordering table the drawer links into
  std::uint32_t meshBytes = 0;
  std::uint32_t primitives = 0;
  std::uint32_t ranges = 0;
  GteControl control{};
};
static_assert(std::is_trivially_copyable_v<StateHeader>);

struct MeshState {
  StateHeader header;
  std::vector<MemoryRange> ranges;
  std::vector<std::byte> bytes;

  static MeshState read(std::span<const std::byte> state) {
    psx::present::StateReader reader(state);
    MeshState mesh;
    mesh.header = reader.get<StateHeader>();
    mesh.ranges = reader.getAll<MemoryRange>(mesh.header.ranges);
    std::size_t total = 0;
    for (const MemoryRange &range : mesh.ranges) {
      total += range.size;
    }
    mesh.bytes = reader.getAll<std::byte>(total);
    return mesh;
  }

  // The same mesh drawn by the same drawer into the same slot table.
  bool sameDraw(const MeshState &other) const {
    return header.call.drawer == other.header.call.drawer && header.call.args[0] == other.header.call.args[0] &&
           header.meshBytes == other.header.meshBytes && header.primitives == other.header.primitives;
  }
};

bool draw(Core &core, const MeshState &state, const GteControl &control, psx::present::PrimitiveSink &sink) {
  HostMemory host;
  host.zero(state.header.tableBase, facts::kOrderingTableBuckets * sizeof(std::uint32_t));
  provideRanges(host, state.ranges, state.bytes);
  {
    const psx::present::GteGuard guard;
    psx::present::writeGteControl(control);
    if (!MeshDrawers::run(EmitMemory(core, host), state.header.call)) {
      return false;
    }
  }
  psx::present::emitHostOrderingTable(host,
                                      psx::present::HostOrderingTable{state.header.tableBase,
                                                                      facts::kOrderingTableBuckets,
                                                                      psx::present::OtSlot{OrderingTables::kTableId, 0},
                                                                      state.header.primitives},
                                      sink);
  return true;
}

} // namespace

// The halfwords the inputs name as list ends are 0 in the saved copy.
void MeshDrawRecorder::endLists(const MeshDrawInputs &inputs, std::vector<std::byte> &bytes) {
  for (const std::uint32_t address : inputs.terminators) {
    std::size_t at = 0;
    for (const MemoryRange &range : inputs.ranges) {
      if (address >= range.address && address + sizeof(std::uint16_t) <= range.address + range.size) {
        bytes[at + (address - range.address)] = std::byte{0};
        bytes[at + (address - range.address) + 1u] = std::byte{0};
        break;
      }
      at += range.size;
    }
  }
}

MeshDrawRecorder::MeshDrawRecorder(Core &core, const MeshDrawCall &call, const MeshDrawInputs &inputs) {
  const EmitMemory guest(core);
  const std::uint32_t table = MeshDrawers::orderingTable(guest, call);
  const std::optional<psx::present::OtSlot> slot = core.otTables.slotOf(table);
  named_ = slot.has_value();
  if (!named_) {
    return;
  }
  StateHeader header;
  header.call = call;
  header.tableBase = table - slot->index * sizeof(std::uint32_t);
  header.meshBytes = inputs.meshBytes;
  header.primitives = inputs.primitives;
  header.ranges = static_cast<std::uint32_t>(inputs.ranges.size());
  header.control = psx::present::readGteControl();
  std::vector<std::byte> bytes = readRanges(guest, inputs.ranges);
  endLists(inputs, bytes);
  psx::present::StateWriter writer;
  writer.put(header);
  writer.putAll(std::span<const MemoryRange>(inputs.ranges));
  writer.putAll(std::span<const std::byte>(bytes));
  const std::span<const std::byte> written = writer.bytes();
  state_.assign(written.begin(), written.end());
}

void MeshDrawRecorder::save(Core &core, const MeshDrawResult &result) const {
  // v0 is 1 when the call linked nothing.
  if (!named_ || result.v0 != 0u) {
    return;
  }
  core.frameStates.save(core.emission.current(), std::span<const std::byte>(state_));
}

// A blend can steer the body onto a path the saved call never took, one with no native body; the saved control
// registers are the call as the guest drew it, which the trial vetted.
void MeshStateRender::render(std::span<const std::byte> from,
                             std::span<const std::byte> to,
                             float t,
                             psx::present::PrimitiveSink &sink) const {
  const MeshState state = MeshState::read(to);
  if (t < 1.0f && from.data() != to.data()) {
    const MeshState earlier = MeshState::read(from);
    if (earlier.sameDraw(state) &&
        draw(core_, state, psx::present::blendGteControl(earlier.header.control, state.header.control, t), sink)) {
      return;
    }
  }
  if (!draw(core_, state, state.header.control, sink)) {
    lucent::error("ts2-mesh", "saved call of 0x{:08X} reaches a path without a native body", state.header.call.drawer);
    std::abort();
  }
}

} // namespace ts2
