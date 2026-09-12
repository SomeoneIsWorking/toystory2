#pragma once

#include <cstdint>
#include <optional>
#include <span>
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

std::uint32_t callGuestToReturn(Core &core, const GuestCall &call);
std::uint32_t callFiniteGuestToReturn(Core &core, const GuestCall &call, std::uint32_t maxSlices);
psx::cpu::ExecutionResult
executeFiniteGuestCall(Core &core, const GuestCall &call, psx::cpu::ExecutionBudget budget, std::uint32_t maxSlices);
void callOriginalToReturn(Core &core, std::uint32_t address, std::string_view owner);
void installResidentOverride(Core &core, std::uint32_t address, std::string_view name, NativeGuestFunction function);

} // namespace ts2
