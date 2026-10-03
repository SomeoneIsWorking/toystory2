#include "frame/field_call.h"

#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {

void FieldCall::begin(Core &core, const GuestCall &call) {
  owner_ = call.owner;
  fields_ = 0;
  publishCallArguments(core, call);
  call_.begin(core, call.owner, call.address, call.returnAddress, psx::cpu::kUnboundedCallTurns);
}

FieldCall::Step FieldCall::advance(Core &core) {
  (void)core;
  for (;;) {
    const psx::cpu::CallStep step = call_.advance();
    switch (step.outcome) {
    case psx::cpu::CallOutcome::Returned:
      lucent::info("ts2-execution",
                   "guest call {} returned after {} display field(s) in {} turn(s)",
                   owner_,
                   fields_,
                   step.turns);
      return Step::returned;
    case psx::cpu::CallOutcome::Suspended:
      if (step.reason == psx::cpu::ExecutionExitReason::BudgetExhausted) {
        // No display field was consumed, so this host step continues the same call.
        continue;
      }
      if (step.reason == psx::cpu::ExecutionExitReason::FrameBoundary) {
        ++fields_;
        return Step::fieldBoundary;
      }
      return Step::hostSlice;
    case psx::cpu::CallOutcome::Refused:
      lucent::error("ts2-execution",
                    "guest call {} at 0x{:08X} refused after {} host turn(s) and {} cycles at 0x{:08X}: {}",
                    owner_,
                    call_.entry(),
                    step.turns,
                    step.cycles,
                    step.guestPc,
                    step.detail);
      std::abort();
    }
  }
}

void FieldCall::giveUp() {
  call_ = psx::cpu::ResumableGuestCall{};
}

} // namespace ts2