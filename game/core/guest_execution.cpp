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
executeFiniteBootCall(Core &core, const GuestCall &call, psx::cpu::ExecutionBudget budget, std::uint32_t maxSlices) {
  if (budget.cycles == 0 || maxSlices == 0) {
    return {psx::cpu::ExecutionExitReason::Fault, call.address, 0, "finite boot call requires a nonzero bound"};
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
    result.detail = "finite boot call exceeded its slice bound";
  }
  lucent::info("ts2-boot",
               "finite guest initializer {} used {} slice(s), {} cycles, exit {} at 0x{:08X}",
               call.owner,
               slices,
               totalCycles,
               psx::cpu::executionExitName(result.reason),
               result.guestPc);
  return result;
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
