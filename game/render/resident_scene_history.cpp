#include "render/resident_scene_history.h"

#include "core.h"
#include "guest_execution.h"
#include "toystory2_context.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

constexpr uint32_t kSceneBank = 0x800A11E0u;
constexpr uint32_t kMaterialDepthTableBase = 0x800A12F8u;

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

void observeSceneOwner(Core *core) {
  ResidentSceneHistory &history = context(*core).scene;
  if (history.capturing()) {
    history.captureOwnerSubmission(*core, core->r[4], core->r[5], core->r[6], core->r[7]);
  }
  callOriginalToReturn(*core, 0x8002622Cu, "resident scene owner original");
}

void observeMeshSubmitter(Core *core) {
  ResidentSceneHistory &history = context(*core).scene;
  if (history.capturing()) {
    history.captureMeshSubmission(*core, core->r[4], core->r[5], core->r[6], core->r[7]);
  }
  callOriginalToReturn(*core, 0x800100E4u, "resident mesh submitter original");
}

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
    Core &core, uint32_t mesh, uint32_t scale, uint32_t materialDepthTableIndex, uint32_t cameraCoarse) {
  if (meshCount_ == meshes_.size()) {
    lucent::error("ts2-scene", "resident mesh capture overflow at {} submissions", meshCount_);
    std::abort();
  }
  ResidentMeshSubmission submission{
      .meshAddress = mesh,
      .headerWord = mesh == 0 ? 0 : static_cast<int32_t>(core.mem_r32(mesh)),
      .scale = scale,
      .materialDepthTableIndex = materialDepthTableIndex,
      .cameraCoarseAddress = cameraCoarse,
  };
  submission.materialDepthTableBase = core.mem_r32(kMaterialDepthTableBase);
  submission.materialDepthTableAddress = submission.materialDepthTableBase + materialDepthTableIndex * 4u;
  submission.materialDepthTableEntry = core.mem_r32(submission.materialDepthTableAddress);
  for (uint32_t index = 0; index < submission.affineControlWords.size(); ++index) {
    submission.affineControlWords[index] = gte_read_ctrl(index);
  }
  for (uint32_t index = 0; index < submission.projectionControlWords.size(); ++index) {
    submission.projectionControlWords[index] = gte_read_ctrl(24u + index);
  }
  submission.textureCoordinateOffset = core.mem_r16(0x1F800024u);
  const std::optional<ResidentMeshLayout> layout = decodeResidentMeshLayout(core, mesh);
  if (layout) {
    const std::optional<ResidentMeshCommand> command = decodeResidentMeshCommand(core, layout->commandAddress);
    const std::optional<ResidentMeshCommandSummary> summary = summarizeResidentMeshCommands(core, *layout);
    if (command && summary) {
      submission.layout = *layout;
      submission.firstCommand = *command;
      submission.commandSummary = *summary;
      submission.materialCensus = censusResidentMeshMaterials(core, *layout, submission.descriptorSamples);
      if (!command->terminal) {
        const std::optional<ResidentMeshPrimitive> primitive = decodeResidentMeshPrimitive(core, *command, 0u);
        if (primitive) {
          submission.firstPrimitive = *primitive;
          submission.decoded = true;
        }
      } else {
        submission.decoded = true;
      }
    }
  }
  meshes_[meshCount_++] = submission;
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
    Core &core, uint32_t mesh, uint32_t scale, uint32_t materialDepthTableIndex, uint32_t cameraCoarse) {
  if (!capturing_) {
    lucent::error("ts2-scene", "resident mesh captured outside a resident update");
    std::abort();
  }
  working_.captureMeshSubmission(core, mesh, scale, materialDepthTableIndex, cameraCoarse);
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
  size_t decodedMeshCount = 0;
  uint32_t primitiveOpcodeMask = 0;
  uint32_t materialTableSlotMask = 0;
  uint8_t blendVariantMask = 0;
  uint64_t primitiveCount = 0;
  uint64_t descriptorCount = 0;
  uint64_t descriptorSampleCount = 0;
  size_t descriptorSampleOverflowCount = 0;
  for (const ResidentMeshSubmission &mesh : meshes) {
    if (!mesh.decoded) {
      continue;
    }
    ++decodedMeshCount;
    primitiveOpcodeMask |= mesh.commandSummary.primitiveOpcodeMask;
    materialTableSlotMask |= mesh.commandSummary.materialTableSlotMask;
    blendVariantMask |= mesh.commandSummary.blendVariantMask;
    primitiveCount += mesh.commandSummary.primitiveCount;
    descriptorCount += mesh.materialCensus.descriptorCount;
    descriptorSampleCount += mesh.materialCensus.sampleCount;
    descriptorSampleOverflowCount += mesh.materialCensus.descriptorSampleOverflow ? 1u : 0u;
  }
  lucent::debug("ts2-scene",
                "resident scene batches={} candidates={} first vis=0x{:08X} type=0x{:02X} "
                "object=0x{:08X} instance=0x{:08X} candidate-mesh=0x{:08X}; meshes={} first "
                "mesh=0x{:08X} header={} vertices={} aux={} command=0x{:04X} opcode={} count={} "
                "depth-index={} depth-base=0x{:08X} depth-address=0x{:08X} depth-entry=0x{:08X}; "
                "decoded={}/{} primitive-count={} descriptor-samples={}/{} overflowed-meshes={} "
                "opcode-mask=0x{:08X} material-slots=0x{:08X} blend-mask=0x{:02X}",
                current_.batches().size(),
                candidates.size(),
                firstCandidate.visibilityAddress,
                firstCandidate.type,
                firstCandidate.objectAddress,
                firstCandidate.instanceAddress,
                firstCandidate.meshAddress,
                meshes.size(),
                firstMesh.meshAddress,
                firstMesh.headerWord,
                firstMesh.layout.vertexCount,
                firstMesh.layout.hasAuxiliaryVertexRecords,
                static_cast<uint16_t>(firstMesh.firstCommand.word),
                firstMesh.firstCommand.opcode,
                firstMesh.firstCommand.primitiveCount,
                firstMesh.materialDepthTableIndex,
                firstMesh.materialDepthTableBase,
                firstMesh.materialDepthTableAddress,
                firstMesh.materialDepthTableEntry,
                decodedMeshCount,
                meshes.size(),
                primitiveCount,
                descriptorSampleCount,
                descriptorCount,
                descriptorSampleOverflowCount,
                primitiveOpcodeMask,
                materialTableSlotMask,
                blendVariantMask);
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

void installResidentSceneObservationOverrides(Core &core) {
  installResidentOverride(core, 0x8002622Cu, "resident-scene-observer", observeSceneOwner);
  installResidentOverride(core, 0x800100E4u, "resident-mesh-observer", observeMeshSubmitter);
}

} // namespace ts2
