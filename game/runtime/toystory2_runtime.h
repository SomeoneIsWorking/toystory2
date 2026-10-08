#pragma once

#include "facts/guest_facts.h"
#include "game_runtime.h"
#include "input/recording_phase.h"

#include <cstdint>

namespace ts2 {

// Declares DeliveryOwner::GuestInterrupt for the CdReadyCallback slot, so the framework's BIOS CD-ROM
// handler stand-in delivers the CD data-ready interrupt.
class ToyStory2Runtime final : public GameRuntime {
public:
  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  const GuestPadBufferLayout *guestPadBufferLayout() const override;
  const GuestCdStreamCallbackLayout *guestCdStreamCallbackLayout() const override;
  const GuestPacketPoolWindows *guestPacketPoolWindows() const override;
  const char *discEnvVar() const override;
  const HostIdentity *hostIdentity() const override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;
  bool sealedFrameIsCut(Core &core) const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  // The title screen discards Start inside its 30-field lockout, so the framework default (absolute pad
  // frames) is wrong here.
  std::uint64_t inputPhase(Core &core) const override {
    return inputPhase_.of(core);
  }

private:
  static constexpr HostIdentity kHostIdentity{facts::kWindowTitle, facts::kCardEnvVar, facts::kCardDefaultPath};
  InputPhase inputPhase_;
};

} // namespace ts2
