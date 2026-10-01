#include "guest_execution.h"

#include "core.h"
#include "execution_exit.h"
#include "guest_call.h"
#include "lightrec_executor.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {

std::uint32_t callGuestToReturn(Core &core, const GuestCall &call) {
  if (call.stackArgument) {
    core.mem_w32(core.r[29] + 16u, *call.stackArgument);
  }
  core.r[31] = call.returnAddress;
  psx::cpu::dispatchGuestWithArgumentsToReturn(
      core, call.address, call.arguments, psx::cpu::ExecutionBudget::currentTurn(core), call.owner);
  return core.r[2];
}

psx::cpu::ExecutionResult
executeFiniteGuestCall(Core &core, const GuestCall &call, psx::cpu::ExecutionBudget budget, std::uint32_t maxSlices) {
  if (budget.cycles == 0 || maxSlices == 0) {
    return {psx::cpu::ExecutionExitReason::Fault, call.address, 0, "finite guest call requires a nonzero bound"};
  }
  if (call.stackArgument) {
    core.mem_w32(core.r[29] + 16u, *call.stackArgument);
  }
  core.r[31] = call.returnAddress;
  auto result = psx::cpu::dispatchGuestWithArguments(core, call.address, call.arguments, budget);
  std::uint64_t totalCycles = result.cycles;
  std::uint32_t slices = 1;
  while (!result.returned() && slices < maxSlices && result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted &&
         result.cycles >= budget.cycles && result.guestPc == core.pc) {
    // Keep the original return sentinel: a nested guest jal may have changed ra when the slice ended.
    result = core.lightrecExecutor().executeFunction(result.guestPc, call.returnAddress, budget);
    totalCycles += result.cycles;
    ++slices;
  }
  result.cycles = totalCycles;
  if (!result.returned() && slices == maxSlices && result.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
    result.detail = "finite guest call exceeded its slice bound";
  }
  if (slices > 1 || !result.returned()) {
    lucent::info("ts2-execution",
                 "finite guest call {} used {} slice(s), {} cycles, exit {} at 0x{:08X}",
                 call.owner,
                 slices,
                 totalCycles,
                 psx::cpu::executionExitName(result.reason),
                 result.guestPc);
  }
  return result;
}

ResumableGuestCall::ResumableGuestCall(Core &core) : core_(core) {}

void ResumableGuestCall::begin(const GuestCall &call) {
  if (active_) {
    lucent::error("ts2-execution", "{} began while {} is still active", call.owner, call_.owner);
    std::abort();
  }
  call_ = call;
  // The caller's argument span may not outlive this call; own a copy.
  arguments_ = {};
  for (std::size_t index = 0; index < call.arguments.size() && index < arguments_.size(); ++index) {
    arguments_[index] = call.arguments[index];
  }
  call_.arguments = std::span<const std::uint32_t>(arguments_.data(), call.arguments.size());
  resumePc_ = call.address;
  fields_ = 0;
  turns_ = 0;
  started_ = false;
  active_ = true;
}

ResumableGuestCall::Progress ResumableGuestCall::advance() {
  if (!active_) {
    return Progress::returned;
  }
  psx::cpu::ExecutionResult result{};
  for (;;) {
    const auto budget = psx::cpu::ExecutionBudget::currentTurn(core_);
    if (!started_) {
      if (call_.stackArgument) {
        core_.mem_w32(core_.r[29] + 16u, *call_.stackArgument);
      }
      core_.r[31] = call_.returnAddress;
      started_ = true;
      result = psx::cpu::dispatchGuestWithArguments(core_, call_.address, call_.arguments, budget);
    } else {
      result = psx::cpu::resumeGuestToReturnFrom(core_, call_.address, resumePc_, call_.returnAddress, budget);
    }
    ++turns_;
    if (result.returned()) {
      active_ = false;
      lucent::info("ts2-execution",
                   "guest call {} returned after {} display field(s) in {} turn(s)",
                   call_.owner,
                   fields_,
                   turns_);
      return Progress::returned;
    }
    resumePc_ = result.guestPc;
    if (result.reason == psx::cpu::ExecutionExitReason::FrameBoundary) {
      ++fields_;
      return Progress::fieldBoundary;
    }
    if (result.reason != psx::cpu::ExecutionExitReason::BudgetExhausted) {
      psx::cpu::requireGuestReturn(result, call_.owner);
      std::abort();
    }
  }
}

std::uint32_t ResumableGuestCall::result() const {
  return core_.r[2];
}

std::uint32_t callFiniteGuestToReturn(Core &core, const GuestCall &call, std::uint32_t maxSlices) {
  if (!psx::cpu::requireGuestReturn(
          executeFiniteGuestCall(core, call, psx::cpu::ExecutionBudget::currentTurn(core), maxSlices), call.owner)) {
    std::abort();
  }
  return core.r[2];
}

void callOriginalToReturn(Core &core, std::uint32_t address, std::string_view owner) {
  psx::cpu::callOriginalToReturn(core, address, psx::cpu::ExecutionBudget::currentTurn(core), owner);
}

void installResidentOverride(Core &core, std::uint32_t address, std::string_view name, NativeGuestFunction function) {
  const auto identity = core.currentImageIdentity(address);
  if (!identity) {
    lucent::error("ts2-overrides",
                  "refused native override '{}' at 0x{:08X}: resident image identity is unavailable",
                  name,
                  address);
    std::abort();
  }
  if (!core.nativeDispatcher().install({{*identity, address}, name, function})) {
    lucent::error("ts2-overrides", "failed to install native override '{}' at 0x{:08X}", name, address);
    std::abort();
  }
}

} // namespace ts2
