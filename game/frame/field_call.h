#pragma once

#include "core.h"
#include "execution/guest_execution.h"
#include "resumable_guest_call.h"

#include <cstdint>
#include <string_view>

namespace ts2 {

// A BudgetExhausted turn consumed no field and continues in the same host step; FrameBoundary and
// CooperativeYield end the step, but only FrameBoundary is a display field.
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

  // One host step. A refusal is fatal: it is logged and the run ends.
  Step advance(Core &core);

  [[nodiscard]] uint32_t result(const Core &core) const {
    return core.r[2];
  }

  [[nodiscard]] uint32_t displayFields() const {
    return fields_;
  }

  // Abandon the call with the guest parked in its field barrier; the caller must not advance it again.
  void giveUp();

private:
  psx::cpu::ResumableGuestCall call_{};
  std::string_view owner_{};
  std::uint32_t fields_ = 0;
};

} // namespace ts2