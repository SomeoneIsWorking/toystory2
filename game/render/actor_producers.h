// The actor renderers as producers: each actor part face is keyed by the actor's incarnation and the
// face's place in its model.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace ts2 {

// 0x8002518C (0x80024174 when the actor is scaled) walks the model's 0x6C-byte part records at 0x800A9618.
// Only a ported drawer opens a key, so packets from unported drawers stay unkeyed.
class ActorProducers {
public:
  inline static constexpr std::uint32_t kActorRenderer = 0x8002518Cu;
  inline static constexpr std::uint32_t kScaledActorRenderer = 0x80024174u;
  inline static constexpr std::uint32_t kPartTable = 0x800A9618u;
  inline static constexpr std::uint32_t kPartBytes = 0x6Cu;
  inline static constexpr std::uint32_t kFaceIndexLimit = 0x10000u;

  static void install(Core &core);

  static std::uint32_t faceElement(std::uint32_t part, std::uint32_t face);

  [[nodiscard]] std::optional<std::uint32_t> drawing() const {
    return drawing_;
  }
  void setDrawing(std::optional<std::uint32_t> object) {
    drawing_ = object;
  }

private:
  std::optional<std::uint32_t> drawing_;
};

} // namespace ts2
