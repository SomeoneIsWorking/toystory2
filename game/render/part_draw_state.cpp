#include "render/part_draw_state.h"

#include "core.h"
#include "emit_memory.h"
#include "gte_control.h"
#include "host_ordering_table.h"
#include "render/ordering_tables.h"
#include "state_bytes.h"

#include "facts/guest_facts.h"

#include <lucent/log.h>

#include <cstring>
#include <optional>

namespace ts2 {
namespace {

using psx::present::EmitMemory;
using psx::present::GteControl;
using psx::present::HostMemory;

constexpr std::uint32_t kScratch = 0x1F800000u;
constexpr std::uint32_t kScratchBytes = 0x400u;
constexpr std::uint32_t kFaceRange = 1u; // inputs() lists the part record, then its face list

struct StateHeader {
  PartDrawCall call;
  std::uint32_t tableBase = 0; // bucket 0 of the ordering table the drawer links into
  std::uint32_t cursor = 0;    // the packet cursor at entry
  std::uint32_t arenaBytes = 0;
  std::uint32_t ranges = 0;
  GteControl control{};
};
static_assert(std::is_trivially_copyable_v<StateHeader>);

struct PartState {
  StateHeader header;
  std::vector<MemoryRange> ranges;
  std::vector<std::byte> bytes;

  static PartState read(std::span<const std::byte> state) {
    psx::present::StateReader reader(state);
    PartState part;
    part.header = reader.get<StateHeader>();
    part.ranges = reader.getAll<MemoryRange>(part.header.ranges);
    std::size_t total = 0;
    for (const MemoryRange &range : part.ranges) {
      total += range.size;
    }
    part.bytes = reader.getAll<std::byte>(total);
    return part;
  }

  // The same face list drawn by the same drawer.
  bool sameDraw(const PartState &other) const {
    return header.call.drawer == other.header.call.drawer && ranges.size() == other.ranges.size() &&
           ranges.size() > kFaceRange && ranges[kFaceRange].address == other.ranges[kFaceRange].address &&
           ranges[kFaceRange].size == other.ranges[kFaceRange].size;
  }
};

} // namespace

PartDrawRecorder::PartDrawRecorder(Core &core, const PartDrawCall &call) : call_(call) {
  const EmitMemory guest(core);
  const std::optional<psx::present::OtSlot> slot = core.otTables.slotOf(guest.mem_r32(PartFaceDrawers::kOrderingTable));
  named_ = slot.has_value();
  if (!named_) {
    return;
  }
  const PartDrawInputs inputs = PartFaceDrawers::inputs(guest, call);
  cursor_ = guest.mem_r32(PartFaceDrawers::kPacketCursor);
  StateHeader header;
  header.call = call;
  header.tableBase = guest.mem_r32(PartFaceDrawers::kOrderingTable) - slot->index * sizeof(std::uint32_t);
  header.cursor = cursor_;
  header.arenaBytes = inputs.packetBytes;
  header.ranges = static_cast<std::uint32_t>(inputs.ranges.size());
  header.control = psx::present::readGteControl();
  psx::present::StateWriter writer;
  writer.put(header);
  writer.putAll(std::span<const MemoryRange>(inputs.ranges));
  std::vector<std::byte> bytes;
  for (const MemoryRange &range : inputs.ranges) {
    for (std::uint32_t offset = 0; offset != range.size; ++offset) {
      bytes.push_back(static_cast<std::byte>(guest.mem_r8(range.address + offset)));
    }
  }
  writer.putAll(std::span<const std::byte>(bytes));
  const std::span<const std::byte> written = writer.bytes();
  state_.assign(written.begin(), written.end());
}

void PartDrawRecorder::save(Core &core) const {
  if (!named_ || core.mem_r32(PartFaceDrawers::kPacketCursor) == cursor_) {
    return;
  }
  core.frameStates.save(core.emission.current(), std::span<const std::byte>(state_));
}

void PartStateRender::render(std::span<const std::byte> from,
                             std::span<const std::byte> to,
                             float t,
                             psx::present::PrimitiveSink &sink) const {
  const PartState state = PartState::read(to);
  GteControl control = state.header.control;
  if (t < 1.0f && from.data() != to.data()) {
    const PartState earlier = PartState::read(from);
    if (earlier.sameDraw(state)) {
      control = psx::present::blendGteControl(earlier.header.control, state.header.control, t);
    }
  }

  HostMemory host;
  host.zero(kScratch, kScratchBytes);
  host.zero(state.header.tableBase, facts::kOrderingTableBuckets * sizeof(std::uint32_t));
  host.zero(state.header.cursor, state.header.arenaBytes);
  std::size_t at = 0;
  for (const MemoryRange &range : state.ranges) {
    const std::span<const std::byte> bytes = std::span(state.bytes).subspan(at, range.size);
    at += range.size;
    // The light vector sits in scratchpad words the render has already provided.
    if (range.address - kScratch < kScratchBytes) {
      host.write(range.address, bytes.data(), range.size);
    } else {
      host.provide(range.address, bytes);
    }
  }

  {
    const psx::present::GteGuard guard;
    psx::present::writeGteControl(control);
    PartFaceDrawers::run(EmitMemory(core_, host), state.header.call);
  }
  psx::present::emitHostOrderingTable(host,
                                      psx::present::HostOrderingTable{state.header.tableBase,
                                                                      facts::kOrderingTableBuckets,
                                                                      psx::present::OtSlot{OrderingTables::kTableId, 0},
                                                                      state.header.arenaBytes / 4u},
                                      sink);
}

} // namespace ts2
