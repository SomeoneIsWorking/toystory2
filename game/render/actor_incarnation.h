// 0x8006A9FC returns an actor to its placement (level start 0x8006ADF8, respawn 0x8006CE84), and each
// call begins a new incarnation, so the two lives of one pooled record never pair.
#pragma once

#include <cstdint>
#include <unordered_map>

class Core;

namespace ts2 {

// The generation sits above the main-RAM offset, so it wraps after 2048 resets of one record.
inline constexpr std::uint32_t kActorOffsetBits = 21u;
inline constexpr std::uint32_t kActorOffsetMask = (1u << kActorOffsetBits) - 1u;

constexpr std::uint32_t incarnationObject(std::uint32_t actor, std::uint32_t generation) {
  return (generation << kActorOffsetBits) | (actor & kActorOffsetMask);
}

class ActorIncarnations {
public:
  inline static constexpr std::uint32_t kActorReset = 0x8006A9FCu;

  // The guest body runs unchanged; only its effect is observed.
  static void install(Core &core);

  void begin(std::uint32_t actor);
  // How many times the record has been reset: the life an object key of it names.
  [[nodiscard]] std::uint32_t generation(std::uint32_t actor) const;

private:
  std::unordered_map<std::uint32_t, std::uint32_t> generations_; // main-RAM offset -> generation
};

} // namespace ts2
