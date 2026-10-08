#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

class Core;

namespace ts2 {

// One guest call: entry, return address, arguments and the owner named in a refusal.
struct GuestCall {
  std::uint32_t address = 0;
  std::uint32_t returnAddress = 0;
  std::span<const std::uint32_t> arguments{};
  // The fifth argument, at `$sp + 16`.
  std::optional<std::uint32_t> stackArgument{};
  std::string_view owner{};
};

inline constexpr std::uint32_t kFiniteInitializationSliceLimit = 64;

// A resident update that outlives a handful of fields is a hang, not slow work.
inline constexpr std::uint32_t kResidentUpdateSliceLimit = 8;

// Entries using this are leaf calls, so one that outlives its first field is refused, not resumed.
inline constexpr std::uint32_t kOneFieldCallTurns = 1;

// Enter `call` and require it to return inside one display field.
std::uint32_t callGuestToReturn(Core &core, const GuestCall &call);

// For a guest body that spans fields: `maxSlices` display fields, refused by name beyond that.
std::uint32_t callFiniteGuestToReturn(Core &core, const GuestCall &call, std::uint32_t maxSlices);

// Publish `$a0`-`$a3` and `$sp + 16`; `ResumableGuestCall::begin` takes only entry and return address.
void publishCallArguments(Core &core, const GuestCall &call);

// Read a NUL-terminated string from guest RAM; no terminator within `maxBytes` reads as empty.
std::string guestString(Core &core, std::uint32_t address, std::size_t maxBytes);

} // namespace ts2