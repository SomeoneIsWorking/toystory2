#include "render/resident_scene_history.h"

#include "core.h"
#include "recomp_iface.h"
#include "toystory2_context.h"

#include <cstdlib>
#include <lucent/log.h>

#ifdef TS2_HAVE_SUBSTRATE
#include "rec_decls.h"
#endif

namespace ts2 {
namespace {

constexpr uint32_t kSceneBank = 0x800A11E0u;

bool usesExtendedInstanceLayout(uint8_t type) {
  switch (type & 0x6Fu) {
  case 9:
  case 0x0C:
  case 0x49:
  case 0x4C:
  case 10:
  case 0x4A:
    return true;
  default:
    return false;
  }
}

uint32_t meshAddress(Core &core, uint32_t instanceAddress, uint8_t type) {
  if (instanceAddress == 0) {
    return 0;
  }
  switch (type & 0x6Fu) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
    return core.mem_r32(instanceAddress + 0x14u);
  case 9:
  case 10:
  case 0x0C:
  case 0x49:
  case 0x4A:
  case 0x4C:
    return core.mem_r32(instanceAddress + 0x1Cu);
  default:
    return 0;
  }
}

ResidentSceneCandidate readCandidate(Core &core, uint32_t visibilityAddress) {
  ResidentSceneCandidate candidate;
  candidate.visibilityAddress = visibilityAddress;
  candidate.type = core.mem_r8(visibilityAddress + 0x0Eu);
  candidate.viewport = core.mem_r8(visibilityAddress + 0x0Fu);
  candidate.objectAddress = core.mem_r32(visibilityAddress + 0x10u);
  if (candidate.objectAddress == 0) {
    return candidate;
  }

  const uint32_t sceneBank = core.mem_r32(kSceneBank);
  candidate.resourceAddress = core.mem_r32(candidate.objectAddress + 0x18u + sceneBank * 4u);
  candidate.instanceAddress = core.mem_r32(candidate.objectAddress + 0x14u);
  if (candidate.instanceAddress == 0) {
    return candidate;
  }

  for (uint32_t axis = 0; axis < 3; ++axis) {
    candidate.position[axis] = static_cast<int32_t>(core.mem_r32(candidate.instanceAddress + axis * 4u));
  }
  const uint32_t materialOffset = usesExtendedInstanceLayout(candidate.type) ? 0x18u : 0x12u;
  candidate.material = core.mem_r8(candidate.instanceAddress + materialOffset);
  candidate.scaleFlags = core.mem_r8(candidate.instanceAddress + materialOffset + 1u);
  candidate.meshAddress = meshAddress(core, candidate.instanceAddress, candidate.type);
  return candidate;
}

#ifdef TS2_HAVE_SUBSTRATE
void observeSceneOwner(Core *core) {
  ResidentSceneHistory &history = context(*core).scene;
  if (history.capturing()) {
    history.captureOwnerSubmission(*core, core->r[4], core->r[5], core->r[6], core->r[7]);
  }
  gen_func_8002622C(core);
}

void observeMeshSubmitter(Core *core) {
  ResidentSceneHistory &history = context(*core).scene;
  if (history.capturing()) {
    const uint32_t mesh = core->r[4];
    const int32_t headerWord = mesh == 0 ? 0 : static_cast<int32_t>(core->mem_r32(mesh));
    history.captureMeshSubmission(mesh, headerWord, core->r[5], core->r[6], core->r[7]);
  }
  gen_func_800100E4(core);
}
#endif

} // namespace

std::span<const ResidentSceneSubmissionBatch> ResidentSceneFrame::batches() const {
  return {batches_.data(), batchCount_};
}

std::span<const ResidentSceneCandidate> ResidentSceneFrame::candidates() const {
  return {candidates_.data(), candidateCount_};
}

std::span<const ResidentMeshSubmission> ResidentSceneFrame::meshes() const {
  return {meshes_.data(), meshCount_};
}

void ResidentSceneFrame::reset() {
  batchCount_ = 0;
  candidateCount_ = 0;
  meshCount_ = 0;
}

void ResidentSceneFrame::captureOwnerSubmission(
    Core &core, uint32_t visibilityList, uint32_t count, uint32_t packetPool, uint32_t viewSelector) {
  if (count > kMaxVisibilityCandidatesPerBatch || batchCount_ == batches_.size() ||
      candidateCount_ + count > candidates_.size()) {
    lucent::error("ts2-scene",
                  "resident scene capture overflow: list=0x{:08X} count={} batches={} candidates={}",
                  visibilityList,
                  count,
                  batchCount_,
                  candidateCount_);
    std::abort();
  }

  ResidentSceneSubmissionBatch &batch = batches_[batchCount_++];
  batch = {.visibilityListAddress = visibilityList,
           .packetPoolAddress = packetPool,
           .viewSelector = viewSelector,
           .candidateBegin = candidateCount_,
           .candidateCount = count};
  for (uint32_t index = 0; index < count; ++index) {
    const uint32_t visibilityAddress = core.mem_r32(visibilityList + index * 4u);
    candidates_[candidateCount_++] = readCandidate(core, visibilityAddress);
  }
}

void ResidentSceneFrame::captureMeshSubmission(
    uint32_t mesh, int32_t headerWord, uint32_t scale, uint32_t materialTable, uint32_t cameraCoarse) {
  if (meshCount_ == meshes_.size()) {
    lucent::error("ts2-scene", "resident mesh capture overflow at {} submissions", meshCount_);
    std::abort();
  }
  meshes_[meshCount_++] = {.meshAddress = mesh,
                           .headerWord = headerWord,
                           .scale = scale,
                           .materialTableAddress = materialTable,
                           .cameraCoarseAddress = cameraCoarse};
}

void ResidentSceneHistory::reset() {
  previous_.reset();
  current_.reset();
  working_.reset();
  capturing_ = false;
  ready_ = false;
}

void ResidentSceneHistory::beginFrame() {
  if (capturing_) {
    lucent::error("ts2-scene", "resident scene capture began before the prior frame finished");
    std::abort();
  }
  working_.reset();
  capturing_ = true;
}

void ResidentSceneHistory::captureOwnerSubmission(
    Core &core, uint32_t visibilityList, uint32_t count, uint32_t packetPool, uint32_t viewSelector) {
  if (!capturing_) {
    lucent::error("ts2-scene", "resident scene owner captured outside a resident update");
    std::abort();
  }
  working_.captureOwnerSubmission(core, visibilityList, count, packetPool, viewSelector);
}

void ResidentSceneHistory::captureMeshSubmission(
    uint32_t mesh, int32_t headerWord, uint32_t scale, uint32_t materialTable, uint32_t cameraCoarse) {
  if (!capturing_) {
    lucent::error("ts2-scene", "resident mesh captured outside a resident update");
    std::abort();
  }
  working_.captureMeshSubmission(mesh, headerWord, scale, materialTable, cameraCoarse);
}

void ResidentSceneHistory::finishFrame() {
  if (!capturing_) {
    lucent::error("ts2-scene", "resident scene capture finished without a matching begin");
    std::abort();
  }
  capturing_ = false;
  if (!ready_) {
    previous_ = working_;
    current_ = working_;
    ready_ = true;
  } else {
    previous_ = current_;
    current_ = working_;
  }
  const std::span<const ResidentSceneCandidate> candidates = current_.candidates();
  const std::span<const ResidentMeshSubmission> meshes = current_.meshes();
  const ResidentSceneCandidate firstCandidate = candidates.empty() ? ResidentSceneCandidate{} : candidates.front();
  const ResidentMeshSubmission firstMesh = meshes.empty() ? ResidentMeshSubmission{} : meshes.front();
  lucent::debug("ts2-scene",
                "resident scene batches={} candidates={} first vis=0x{:08X} type=0x{:02X} "
                "object=0x{:08X} instance=0x{:08X} candidate-mesh=0x{:08X}; meshes={} first "
                "mesh=0x{:08X} header={}",
                current_.batches().size(),
                candidates.size(),
                firstCandidate.visibilityAddress,
                firstCandidate.type,
                firstCandidate.objectAddress,
                firstCandidate.instanceAddress,
                firstCandidate.meshAddress,
                meshes.size(),
                firstMesh.meshAddress,
                firstMesh.headerWord);
}

bool ResidentSceneHistory::capturing() const {
  return capturing_;
}

bool ResidentSceneHistory::ready() const {
  return ready_;
}

const ResidentSceneFrame &ResidentSceneHistory::previous() const {
  if (!ready_) {
    std::abort();
  }
  return previous_;
}

const ResidentSceneFrame &ResidentSceneHistory::current() const {
  if (!ready_) {
    std::abort();
  }
  return current_;
}

void installResidentSceneObservationOverrides() {
#ifdef TS2_HAVE_SUBSTRATE
  const RecompRegistry *const registry = psxport_recomp();
  if (registry == nullptr || registry->shard_set_override == nullptr) {
    lucent::error("ts2-scene", "generated override registry is unavailable");
    std::abort();
  }
  registry->shard_set_override(0x8002622Cu, observeSceneOwner);
  registry->shard_set_override(0x800100E4u, observeMeshSubmitter);
#endif
}

} // namespace ts2
