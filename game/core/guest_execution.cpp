#include "guest_execution.h"

#include "core.h"
#include "execution_exit.h"
#include "guest_call.h"
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
