#pragma once

#include "core.h"
#include "execution/guest_execution.h"
#include "resumable_guest_call.h"

#include <cstdint>
#include <string_view>

namespace ts2 {

// One guest call the FRAME TURN drives across display fields. The loop, the return-address latch, the
// turn cap and the classification of a stop are `psx::cpu::ResumableGuestCall`; what this owner adds
// is this title's policy about a host step:
//
//   * a turn that ended BudgetExhausted consumed no display field, so the SAME host step continues
//     it — the guest simply ran out of cycles inside the field it was already going to be given;
//   * a turn that ended FrameBoundary is a display field and ends the step;
//   * a turn that ended CooperativeYield is a native replacement handing back one slice of work, which
//     ends the step too but is NOT a display field;
//   * the call's arguments are published before it begins, and the display fields it has waited on are
//     counted and logged when it returns.
//
// `begin` states no turn cap (`kUnboundedCallTurns`): the frame loop is what bounds a field call, and
// its finite siblings carry this title's measured slice caps instead
// (`kFiniteInitializationSliceLimit`, `kResidentUpdateSliceLimit`).
class FieldCall {
public:
  enum class Step {
    fieldBoundary, // the guest reached its own field barrier; present a display field
    hostSlice,     // a native replacement finished a slice; present a display field
    returned,      // the call reached its return address; `result()` is its `$v0`
  };

  void begin(Core &core, const GuestCall &call);

  [[nodiscard]] bool pending() const {
    return call_.pending();
  }

  // One host step of the call. A refusal is reported here and the call is left as the framework left
  // it: a frame call that cannot continue is fatal, so this names the reason and ends the run.
  Step advance(Core &core);

  [[nodiscard]] uint32_t result(const Core &core) const {
    return core.r[2];
  }

  [[nodiscard]] uint32_t displayFields() const {
    return fields_;
  }

  // Give the call up where it stopped, leaving the guest parked inside its own field barrier. Used
  // only where the guest's own loop, not this call, owns what comes next; the caller must not advance
  // it afterwards.
  void giveUp();

private:
  psx::cpu::ResumableGuestCall call_{};
  std::string_view owner_{};
  std::uint32_t fields_ = 0;
};

} // namespace ts2