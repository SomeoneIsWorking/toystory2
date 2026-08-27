#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace ts2 {

// 0x80027724 allocates two 256-entry merge arrays and receives the same culler count later passed to
// owner 0x8002622C. A larger batch would already violate the retail owner's stack contract.
inline constexpr size_t kMaxVisibilityCandidatesPerBatch = 256;
inline constexpr size_t kMaxSceneSubmissionBatches = 4;
inline constexpr size_t kMaxSceneMeshSubmissions = 8192;

struct ResidentSceneCandidate {
  uint32_t visibilityAddress = 0;
  uint32_t objectAddress = 0;
  uint32_t resourceAddress = 0;
  uint32_t instanceAddress = 0;
  uint32_t meshAddress = 0;
  int32_t position[3] = {};
  uint8_t type = 0;
  uint8_t viewport = 0;
  uint8_t material = 0;
  uint8_t scaleFlags = 0;
};

struct ResidentSceneSubmissionBatch {
  uint32_t visibilityListAddress = 0;
  uint32_t packetPoolAddress = 0;
  uint32_t viewSelector = 0;
  size_t candidateBegin = 0;
  size_t candidateCount = 0;
};

struct ResidentMeshSubmission {
  uint32_t meshAddress = 0;
  int32_t headerWord = 0;
  uint32_t scale = 0;
  uint32_t materialTableAddress = 0;
  uint32_t cameraCoarseAddress = 0;
};

class ResidentSceneFrame {
public:
  std::span<const ResidentSceneSubmissionBatch> batches() const;
  std::span<const ResidentSceneCandidate> candidates() const;
  std::span<const ResidentMeshSubmission> meshes() const;

private:
  friend class ResidentSceneHistory;

  void reset();
  void captureOwnerSubmission(
      Core &core, uint32_t visibilityList, uint32_t count, uint32_t packetPool, uint32_t viewSelector);
  void captureMeshSubmission(
      uint32_t mesh, int32_t headerWord, uint32_t scale, uint32_t materialTable, uint32_t cameraCoarse);

  std::array<ResidentSceneSubmissionBatch, kMaxSceneSubmissionBatches> batches_{};
  std::array<ResidentSceneCandidate, kMaxVisibilityCandidatesPerBatch * kMaxSceneSubmissionBatches> candidates_{};
  std::array<ResidentMeshSubmission, kMaxSceneMeshSubmissions> meshes_{};
  size_t batchCount_ = 0;
  size_t candidateCount_ = 0;
  size_t meshCount_ = 0;
};

class ResidentSceneHistory {
public:
  void reset();
  void beginFrame();
  void captureOwnerSubmission(
      Core &core, uint32_t visibilityList, uint32_t count, uint32_t packetPool, uint32_t viewSelector);
  void captureMeshSubmission(
      uint32_t mesh, int32_t headerWord, uint32_t scale, uint32_t materialTable, uint32_t cameraCoarse);
  void finishFrame();

  bool capturing() const;
  bool ready() const;
  const ResidentSceneFrame &previous() const;
  const ResidentSceneFrame &current() const;

private:
  ResidentSceneFrame previous_{};
  ResidentSceneFrame current_{};
  ResidentSceneFrame working_{};
  bool capturing_ = false;
  bool ready_ = false;
};

// Runtime observation wrappers preserve the generated owners as super-calls. They record the exact
// 0x8002622C candidate batches and 0x800100E4 mesh arguments without changing guest state.
void installResidentSceneObservationOverrides();

} // namespace ts2
