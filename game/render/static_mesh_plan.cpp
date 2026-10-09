// What a static submitter call reads and writes, and the trial that vets it. The ranges follow the mesh's face
// entries: each retained packet and the quadtree of subdivided children below it, in both packet arrays, plus
// windows on the free and released slot lists.
#include "facts/guest_facts.h"
#include "gte_control.h"
#include "render/mesh_format.h"
#include "render/slot_packets.h"
#include "render/static_mesh_drawer.h"
#include "render/static_mesh_layout.h"

#include <optional>
#include <vector>

namespace ts2 {

using psx::present::EmitMemory;
using psx::present::HostMemory;

namespace {

using namespace mesh_scratch;

constexpr std::uint32_t kPacketBytes = 0x40u;
constexpr std::uint32_t kMaxPrimitives = 0x10000u;
constexpr std::uint32_t kMaxTreeNodes = 4096u;
constexpr std::uint32_t kMaxBucketBytes = 0x10000u;
constexpr std::uint32_t kFreeSlack = 96u;    // free slots a trial may take beyond one per unslotted face
constexpr std::uint32_t kReleasedSlack = 4u; // the released list's end is not exact; this much room past it
constexpr std::uint32_t kChildFlag = 39u;

struct Gathered {
  MeshDrawInputs inputs;
  std::uint32_t freeCursor = 0;
  std::uint32_t releasedCursor = 0;
  std::uint32_t freeIds = 0; // ids the free-list window covers
};

class Gather {
public:
  Gather(const EmitMemory &memory, const MeshDrawCall &call) : m_(memory), call_(call) {}

  // The ranges for a call whose free-list window holds `freeIds` ids (by default one per unslotted face plus
  // slack); nullopt when the mesh is not one the native body handles.
  std::optional<Gathered> run(std::optional<std::uint32_t> freeIds);

private:
  bool tree(std::uint32_t slot, bool textured);
  void packet(std::uint32_t slot);
  void add(std::uint32_t address, std::uint32_t size) {
    ranges_.push_back({address, size});
  }

  const EmitMemory &m_;
  const MeshDrawCall &call_;
  std::vector<MemoryRange> ranges_;
  std::uint32_t packets_ = 0;
  std::uint32_t alt_ = 0;
  std::uint32_t nodes_ = 0;
};

void Gather::packet(std::uint32_t slot) {
  add(packets_ + slot * 4u, kPacketBytes);
  add(alt_ + slot * 4u, kPacketBytes);
}

// A retained packet and, while its flag says it was subdivided, every child below it. A negative flag puts the
// child ids in the other array's copy.
bool Gather::tree(std::uint32_t slot, bool textured) {
  if (++nodes_ > kMaxTreeNodes) {
    return false;
  }
  packet(slot);
  const std::int32_t flag = m_.mem_r8s(packets_ + slot * 4u + kChildFlag);
  if (flag == 0) {
    return true;
  }
  const std::uint32_t at = (flag < 0 ? alt_ : packets_) + slot * 4u;
  const std::uint32_t first = m_.mem_r32(at + 8u);
  const std::uint32_t second = m_.mem_r32(at + (textured ? 20u : 16u));
  for (const std::uint32_t child : {first & 0x7FFFu, first >> 16, second & 0x7FFFu, second >> 16}) {
    if (!tree(child, textured)) {
      return false;
    }
  }
  return true;
}

std::optional<Gathered> Gather::run(std::optional<std::uint32_t> freeIds) {
  const std::uint32_t mesh = call_.args[0];
  const std::optional<ResidentMeshStream> stream = walkResidentMesh(m_, mesh);
  if (!stream || stream->layout.hasAuxiliaryVertexRecords || stream->primitives >= kMaxPrimitives) {
    return std::nullopt;
  }
  for (const ResidentMeshCommand &command : stream->commands) {
    if (!command.terminal && !StaticMeshDrawer::handles(command.opcode)) {
      return std::nullopt;
    }
  }
  const std::uint32_t tables = m_.mem_r32(kBucketTables);
  const std::uint32_t bucketBytes = m_.mem_r32(kOtzLimit) << 2;
  if (bucketBytes > kMaxBucketBytes) {
    return std::nullopt;
  }
  packets_ = m_.mem_r32(kSlotPacketBase);
  alt_ = m_.mem_r32(kAltPackets);
  const std::uint32_t table = m_.mem_r32(kInstanceSlotTable);
  add(mesh, stream->end - mesh);
  add(call_.args[3] + 0xCu, sizeof(std::uint32_t));
  for (const std::uint32_t global : {kBucketTables, kOtzLimit, kInstanceSlotTable}) {
    add(global, sizeof(std::uint32_t));
  }
  add(tables + (call_.args[2] << 2), bucketBytes + sizeof(std::uint16_t));
  add(kTexturePages, kTexturePageBytes);
  add(table, stream->primitives * 2u);
  add(kMeshScratch, kMeshScratchBytes);

  std::uint32_t unslotted = 0;
  std::uint32_t slotted = 0;
  std::uint32_t entry = 0;
  for (const ResidentMeshCommand &command : stream->commands) {
    for (std::uint32_t i = 0; !command.terminal && i != command.primitiveCount; ++i, ++entry) {
      const std::uint32_t slot = m_.mem_r16(table + entry * 2u);
      if (slot == 0u) {
        ++unslotted;
      } else {
        ++slotted;
        if (!tree(slot, command.opcode < 16u)) {
          return std::nullopt;
        }
      }
    }
  }

  Gathered out;
  out.freeCursor = m_.mem_r32(kFree);
  out.releasedCursor = m_.mem_r32(kReleased);
  out.freeIds = freeIds.value_or(unslotted + kFreeSlack);
  add(out.freeCursor, (out.freeIds + 1u) * 2u);
  for (std::uint32_t i = 0; i != out.freeIds; ++i) {
    const std::uint32_t slot = m_.mem_r16(out.freeCursor + i * 2u);
    if (slot == 0u) {
      break;
    }
    packet(slot);
  }
  add(out.releasedCursor, (slotted + nodes_ + out.freeIds + kReleasedSlack + 1u) * 2u);

  out.inputs.meshBytes = stream->end - mesh;
  out.inputs.primitives = stream->primitives;
  out.inputs.ranges = unionOf(std::move(ranges_));
  out.inputs.terminators.push_back(out.freeCursor + out.freeIds * 2u);
  return out;
}

// The trial: the call run over host copies of its ranges, the table it links into empty. Its free-list window
// ends in a 0, so a call that wants more slots than the window holds stops short and is refused.
std::optional<std::uint32_t> trial(const EmitMemory &guest, const MeshDrawCall &call, const Gathered &gathered) {
  HostMemory host;
  std::vector<std::byte> bytes = readRanges(guest, gathered.inputs.ranges);
  provideRanges(host, gathered.inputs.ranges, bytes);
  const std::uint16_t end = 0;
  for (const std::uint32_t at : gathered.inputs.terminators) {
    host.write(at, &end, sizeof(end));
  }
  host.zero(StaticMeshDrawer::orderingTable(guest), facts::kOrderingTableBuckets * sizeof(std::uint32_t));
  const EmitMemory over(guest.core(), host);
  const psx::present::GteGuard gte;
  if (!StaticMeshDrawer::run(over, call)) {
    return std::nullopt;
  }
  return (over.mem_r32(kFree) - gathered.freeCursor) / 2u;
}

} // namespace

std::optional<MeshDrawInputs> StaticMeshDrawer::plan(const EmitMemory &memory, const MeshDrawCall &call) {
  const std::optional<Gathered> wide = Gather(memory, call).run(std::nullopt);
  if (!wide) {
    return std::nullopt;
  }
  const std::optional<std::uint32_t> used = trial(memory, call, *wide);
  if (!used || *used >= wide->freeIds) {
    return std::nullopt;
  }
  std::optional<Gathered> exact = Gather(memory, call).run(*used);
  if (!exact) {
    return std::nullopt;
  }
  return std::move(exact->inputs);
}

} // namespace ts2
