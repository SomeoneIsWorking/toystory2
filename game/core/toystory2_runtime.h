#pragma once

#include "game_runtime.h"
#include "guest_facts.h"
#include "input/toystory2_input_phase.h"

#include <cstdint>

namespace ts2 {

// Process-lifetime owner of Toy Story 2's framework-facing behavior. It derives GameRuntime directly
// and declares each measured guest fact group (guest_facts.h) through the framework's typed seams, so
// the framework runs it as a direct runtime: no GameConfig, no GameHooks. In particular the CD
// data-ready interrupt is delivered by the framework's stand-in for the BIOS CD-ROM handler, because
// this runtime declares DeliveryOwner::GuestInterrupt for the guest's CdReadyCallback slot.
class ToyStory2Runtime final : public GameRuntime {
public:
  ToyStory2Runtime();
  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  const GuestPadBufferLayout *guestPadBufferLayout() const override;
  const GuestCdStreamCallbackLayout *guestCdStreamCallbackLayout() const override;
  const char *discEnvVar() const override;
  const HostIdentity *hostIdentity() const override;
  RenderCapabilities renderCapabilities() const override;
  // fps60 is created HERE, by the runtime, and nowhere else: a direct runtime's default returns
  // nullptr, so without this the capability declares interpolation that no presenter exists to
  // perform, and the request silently produces the 30fps path.
  std::unique_ptr<TemporalFramePresentation> createTemporalFramePresentation(Game &game) override;
  bool guestVramIsPicture(const Game &game) const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  // The pad recording's phase key (game/input/toystory2_input_phase.h). Declared HERE rather than left at the
  // framework's default, because a recording keyed on absolute pad frames is what sent every earlier
  // route into attract: the title screen discards a Start press inside its own 30-field lockout.
  std::uint64_t inputPhase(Core &core) const override {
    return inputPhase_.of(core);
  }

private:
  static constexpr HostIdentity kHostIdentity{facts::kWindowTitle, facts::kCardEnvVar, facts::kCardDefaultPath};
  InputPhase inputPhase_;
};

} // namespace ts2
