#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

class Core;
namespace psx::cpu {
struct ExecutionBudget;
struct ExecutionResult;
} // namespace psx::cpu

namespace ts2 {

using NativeGuestFunction = void (*)(Core *);

struct GuestCall {
  std::uint32_t address = 0;
  std::uint32_t returnAddress = 0;
  std::span<const std::uint32_t> arguments{};
  std::optional<std::uint32_t> stackArgument{};
  std::string_view owner{};
};

inline constexpr std::uint32_t kFiniteInitializationSliceLimit = 64;

// The first resident update of a level builds its scene and measured 674,080 cycles (1.19 display
// fields); every later update costs about 0.8 field. One update that has not returned within eight
// fields (4.5M cycles) is a hang, not a heavy frame, and fails closed.
inline constexpr std::uint32_t kResidentUpdateSliceLimit = 8;

// One guest call that spans display fields: an FMV overlay's whole-movie loop waits on VSync once per
// movie frame, and each wait exits the executor as a `FrameBoundary`. A native player that does one
// movie frame per host turn yields instead, and yields are the same thing from here — the host has
// done one slice of work and the frame loop should take its field. `advance` runs the call until the
// next such boundary or its return, resuming from the guest's own continuation with the original
// return sentinel, so the host can present one field per step. Any other exit than a budget turn
// ending is a fault and terminates.
class ResumableGuestCall {
public:
  // fieldBoundary: the guest reached a display field (VSync, or its own wait). hostSlice: a native
  // replacement did one slice of work and yielded the turn. Both mean "one step, then present a
  // field"; they are distinct because only the first is a guest display field and only it counts.
  enum class Progress { fieldBoundary, hostSlice, returned };

  explicit ResumableGuestCall(Core &core);

  void begin(const GuestCall &call);
  bool active() const {
    return active_;
  }
  Progress advance();
  // Give the call up at the field boundary it stopped on, leaving the guest parked inside its own
  // field barrier. Used only where the guest's own loop, not this call, owns what comes next; the
  // caller must not advance it afterwards.
  void abandon() {
    active_ = false;
  }
  // The guest `$v0` of the call that last returned.
  std::uint32_t result() const;

private:
  Core &core_;
  GuestCall call_{};
  std::array<std::uint32_t, 4> arguments_{};
  std::uint32_t resumePc_ = 0;
  std::uint32_t fields_ = 0;
  std::uint32_t turns_ = 0;
  bool started_ = false;
  bool active_ = false;
};

std::uint32_t callGuestToReturn(Core &core, const GuestCall &call);
std::uint32_t callFiniteGuestToReturn(Core &core, const GuestCall &call, std::uint32_t maxSlices);
psx::cpu::ExecutionResult
executeFiniteGuestCall(Core &core, const GuestCall &call, psx::cpu::ExecutionBudget budget, std::uint32_t maxSlices);
// Read a NUL-terminated string out of guest RAM. `maxBytes` is the longest name this title's guest
// ever spells; a string with no terminator inside that bound is not one of them and reads as empty.
std::string guestString(Core &core, std::uint32_t address, std::size_t maxBytes);
void callOriginalToReturn(Core &core, std::uint32_t address, std::string_view owner);
// The same call for a guest body that may legitimately need more host turns than one: it resumes the
// original across bounded turns (kMaxResumedHostTurns) and refuses, by name, if it never returns.
void callOriginalToReturnResuming(Core &core, std::uint32_t address, std::string_view owner);
void installResidentOverride(Core &core, std::uint32_t address, std::string_view name, NativeGuestFunction function);

} // namespace ts2
