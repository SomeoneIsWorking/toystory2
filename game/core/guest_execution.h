#pragma once

#include <array>
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

// One guest call that spans display fields: an FMV overlay's whole-movie loop waits on VSync once per
// movie frame, and each wait exits the executor as a `FrameBoundary`. `advance` runs the call until the
// next such boundary or its return, resuming from the guest's own continuation with the original
// return sentinel, so the host can present one field per step. Any other exit than a budget turn
// ending is a fault and terminates.
class ResumableGuestCall {
public:
  enum class Progress { fieldBoundary, returned };

  explicit ResumableGuestCall(Core &core);

  void begin(const GuestCall &call);
  bool active() const {
    return active_;
  }
  Progress advance();
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
void callOriginalToReturn(Core &core, std::uint32_t address, std::string_view owner);
void installResidentOverride(Core &core, std::uint32_t address, std::string_view name, NativeGuestFunction function);

} // namespace ts2
