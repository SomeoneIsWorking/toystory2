// The actor renderers as producers: each actor part is a state producer object, named by the actor's
// incarnation and the part record, and each of its faces an element of it.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace ts2 {

// 0x8002518C (0x80024174 when the actor is scaled) walks the model's 0x6C-byte part records at 0x800A9618.
// Only a ported drawer opens a scope, so packets from unported drawers stay unkeyed. The renderer cannot hold
// one scope for the whole actor, because every unported drawer's stores under it would bind to the actor and be
// replaced by the ported parts' render; each part is its own object instead, saved once per drawer call.
class ActorProducers {
public:
  inline static constexpr std::uint32_t kActorRenderer = 0x8002518Cu;
  inline static constexpr std::uint32_t kScaledActorRenderer = 0x80024174u;
  inline static constexpr std::uint32_t kPartTable = 0x800A9618u;
  inline static constexpr std::uint32_t kPartBytes = 0x6Cu;

  static void install(Core &core);

  // The part record's object key under the actor's current life.
  static std::uint32_t partObject(std::uint32_t part, std::uint32_t generation);

  // The life of the actor being drawn, while a renderer runs.
  [[nodiscard]] std::optional<std::uint32_t> drawing() const {
    return drawing_;
  }
  void setDrawing(std::optional<std::uint32_t> generation) {
    drawing_ = generation;
  }

private:
  std::optional<std::uint32_t> drawing_;
};

} // namespace ts2
