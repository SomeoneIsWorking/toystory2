#pragma once

#include "game_runtime.h"
#include "guest_facts.h"

namespace ts2 {

// Process-lifetime owner of Toy Story 2's framework-facing behavior. It derives GameRuntime directly
// and declares each measured guest fact group (guest_facts.h) through the framework's typed seams, so
// the framework runs it as a direct runtime: no GameConfig, no GameHooks. In particular the CD
// data-ready interrupt is delivered by the framework's stand-in for the BIOS CD-ROM handler, because
// this runtime declares DeliveryOwner::GuestInterrupt for the guest's CdReadyCallback slot.
class ToyStory2Runtime final : public GameRuntime {
public:
  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  const GuestPadBufferLayout *guestPadBufferLayout() const override;
  const GuestCdStreamCallbackLayout *guestCdStreamCallbackLayout() const override;
  const char *discEnvVar() const override;
  const HostIdentity *hostIdentity() const override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;

private:
  static constexpr HostIdentity kHostIdentity{
      facts::kWindowTitle, facts::kCardEnvVar, facts::kCardDefaultPath};
};

} // namespace ts2
