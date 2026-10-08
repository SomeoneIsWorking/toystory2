// A record is a cut when another level (0x800A16A8) or area table (0x800A1274) was shown, a camera was
// placed afresh (FUN_80065CD0), or an update took the camera from another source with no blend pending.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace ts2 {

class FrameCut {
public:
  inline static constexpr std::uint32_t kLevelId = 0x800A16A8u;
  inline static constexpr std::uint32_t kAreaObjects = 0x800A1274u;
  inline static constexpr std::uint32_t kCameraPlacement = 0x80065CD0u;
  inline static constexpr std::uint32_t kCameraUpdate = 0x80068E2Cu;
  inline static constexpr std::uint32_t kCameraSource = 0x800B2256u;
  inline static constexpr std::uint32_t kCameraBlendFields = 0x800A1498u;

  // Both guest bodies run unchanged; only their effect is observed.
  static void install(Core &core);

  void noteCameraPlaced() {
    cameraPlaced_ = true;
  }
  void noteCameraUpdate(bool direct, bool lookCamera);
  // The update that built the next record has ended.
  void notePassEnded(Core &core);
  [[nodiscard]] bool isCut() const {
    return cut_;
  }

private:
  struct SceneIdentity {
    std::uint32_t level = 0;
    std::uint32_t areaObjects = 0;
    bool operator==(const SceneIdentity &) const = default;
  };

  std::optional<SceneIdentity> previous_;
  std::optional<bool> lookCamera_;
  bool cameraPlaced_ = false;
  bool cameraSwitched_ = false;
  bool cut_ = true;
};

} // namespace ts2
