#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

class Core;

namespace ts2 {

// The facts of ONE guest call this title makes: where it enters, the address that ends it, the
// arguments it is entered with, and the owner that names it in a refusal. The loop that runs it across
// host turns, the return-address latch, the turn caps, the refusals, the classification of a stop and
// the per-Core census are `psx::cpu::ResumableGuestCall` and `Core::guestCallCensus()`.
struct GuestCall {
  std::uint32_t address = 0;
  std::uint32_t returnAddress = 0;
  std::span<const std::uint32_t> arguments{};
  // The guest's caller put an argument in the stack slot at `$sp + 16`, which is where a PSX call's
  // fifth argument lives. None of this title's calls sets it; the field exists because the guest
  // reads that slot when one is passed.
  std::optional<std::uint32_t> stackArgument{};
  std::string_view owner{};
};

inline constexpr std::uint32_t kFiniteInitializationSliceLimit = 64;

// The first resident update of a level builds its scene and measured 674,080 cycles (1.19 display
// fields); every later update costs about 0.8 field. One update that has not returned within eight
// fields (4.5M cycles) is a hang, not a heavy frame, and fails closed.
inline constexpr std::uint32_t kResidentUpdateSliceLimit = 8;

// One display field, required to return. The cap is the TITLE's check: these entries are proven leaf
// calls, so a call that does not return inside its first field is refused by name here instead of
// being resumed. `psx::cpu::ResumableGuestCall` is the loop; `kOneFieldCallTurns` is this fact.
inline constexpr std::uint32_t kOneFieldCallTurns = 1;

// Enter `call` and require it to return inside ONE display field, with its arguments published first.
std::uint32_t callGuestToReturn(Core &core, const GuestCall &call);

// The same call for a guest body that is measured to span fields: `maxSlices` display fields, and a
// refusal by name if it outlives them.
std::uint32_t callFiniteGuestToReturn(Core &core, const GuestCall &call, std::uint32_t maxSlices);

// Publish the arguments a `GuestCall` is entered with. Every resumable call in this title begins by
// saying so, because `ResumableGuestCall::begin` deliberately takes only the entry and the return
// address: what a guest function reads in `$a0`-`$a3` and at `$sp + 16` is the caller's fact.
void publishCallArguments(Core &core, const GuestCall &call);

// Read a NUL-terminated string out of guest RAM. `maxBytes` is the longest name this title's guest
// ever spells; a string with no terminator inside that bound is not one of them and reads as empty.
std::string guestString(Core &core, std::uint32_t address, std::size_t maxBytes);

} // namespace ts2