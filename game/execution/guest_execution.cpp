#include "execution/guest_execution.h"

#include "core.h"
#include "resumable_guest_call.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {

void publishCallArguments(Core &core, const GuestCall &call) {
  if (call.stackArgument) {
    core.mem_w32(core.r[29] + 16u, *call.stackArgument);
  }
  for (std::size_t index = 0; index < call.arguments.size() && index < 4; ++index) {
    core.r[4 + index] = call.arguments[index];
  }
}

std::uint32_t callGuestToReturn(Core &core, const GuestCall &call) {
  publishCallArguments(core, call);
  psx::cpu::ResumableGuestCall guestCall;
  guestCall.begin(core, call.owner, call.address, call.returnAddress, kOneFieldCallTurns);
  const psx::cpu::CallStep step = guestCall.advance();
  if (step.outcome != psx::cpu::CallOutcome::Returned) {
    lucent::error("ts2-execution",
                  "guest call {} at 0x{:08X} did not return inside one display field: {} ({} turn(s), "
                  "{} cycles, stopped at 0x{:08X})",
                  call.owner,
                  call.address,
                  step.detail,
                  step.turns,
                  step.cycles,
                  step.guestPc);
    std::abort();
  }
  return step.value;
}

std::uint32_t callFiniteGuestToReturn(Core &core, const GuestCall &call, std::uint32_t maxSlices) {
  publishCallArguments(core, call);
  return psx::cpu::callGuestToReturnResuming(
      core, call.owner, call.address, call.returnAddress, std::nullopt, maxSlices);
}

std::string guestString(Core &core, std::uint32_t address, std::size_t maxBytes) {
  if (address == 0) {
    return {};
  }
  std::string text;
  for (std::size_t offset = 0; offset < maxBytes; ++offset) {
    const char c = static_cast<char>(core.mem_r8(address + static_cast<std::uint32_t>(offset)));
    if (c == 0) {
      return text;
    }
    text.push_back(c);
  }
  return {};
}

} // namespace ts2