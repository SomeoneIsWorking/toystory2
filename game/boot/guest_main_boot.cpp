#include "boot/guest_main_boot.h"

#include "core.h"
#include "guest_execution.h"

#include <array>
#include <cstdint>
#include <lucent/log.h>

namespace ts2 {
namespace {

constexpr uint32_t kGuestMain = 0x8007A9E8u;
constexpr uint32_t kMainLibcInit = 0x80082D58u;
constexpr uint32_t kGraphicsInit = 0x8003A780u;
constexpr uint32_t kLoadMemoryOverlay = 0x8003FCE8u;
constexpr uint32_t kInitializeMemoryOverlay = 0x8003FCB8u;
constexpr uint32_t kInitializeDisplayMode = 0x80073018u;

constexpr uint32_t kBootMode = 0x800C1668u;
constexpr uint32_t kBootCountdown = 0x800C166Cu;
constexpr uint32_t kBootSelection = 0x800C1670u;
constexpr uint32_t kBootFlags = 0x800C1674u;
constexpr uint32_t kFadeCountdown = 0x800A1430u;
constexpr uint32_t kMemoryMode = 0x800C1610u;
constexpr uint32_t kMemoryFlags = 0x800C160Cu;
constexpr uint32_t kMemoryVariant = 0x800C1612u;
constexpr uint32_t kDisplayWidthMode = 0x800C160Du;
constexpr uint32_t kDisplayHeightMode = 0x800C160Eu;
constexpr uint32_t kPlaybackMode = 0x800A1670u;
constexpr uint32_t kMenuPhase = 0x800A14D8u;
constexpr uint32_t kLoopExitReason = 0x800A136Eu;

void callGuest(Core &core, uint32_t address, uint32_t returnAddress, uint32_t a0 = 0, uint32_t a1 = 0) {
  const std::array arguments{a0, a1};
  callGuestToReturn(core, {address, returnAddress, arguments, std::nullopt, "Toy Story 2 boot"});
}

} // namespace

void initializeGuestMain(Core &core) {
  // Preserve the guest main's live stack frame: the original never unwinds it because its outer loop
  // never returns. Finite title steps continue using the same guest stack below this frame.
  core.r[29] -= 48;
  core.mem_w32(core.r[29] + 40, core.r[31]);
  core.mem_w32(core.r[29] + 36, core.r[17]);
  core.mem_w32(core.r[29] + 32, core.r[16]);

  callGuest(core, kMainLibcInit, 0x8007A9FCu);

  core.mem_w32(kBootMode, 0);
  core.mem_w32(kBootSelection, 2);
  core.mem_w32(kBootCountdown, static_cast<uint32_t>(-30));
  core.mem_w32(kBootFlags, 0);
  core.mem_w32(kFadeCountdown, static_cast<uint32_t>(-60));

  callGuest(core, kGraphicsInit, 0x8007AA64u);

  core.r[16] = kMemoryMode;
  core.mem_w16(kMemoryMode, 1);
  core.mem_w8(kMemoryFlags, 0xC0);
  core.mem_w16(kPlaybackMode, 0);
  core.mem_w32(kMenuPhase, 0);
  core.mem_w16(kLoopExitReason, 0);
  core.mem_w16(kMemoryVariant, 0);
  core.mem_w8(kDisplayWidthMode, 8);
  core.mem_w8(kDisplayHeightMode, 8);

  lucent::info("ts2-boot",
               "guest main 0x{:08X} synchronous prefix returned; finite FrameDriver boot owns "
               "overlay initialization and the outer loop",
               kGuestMain);
}

void finishGuestMainBoot(Core &core) {
  // MEMORY initialization may use the measured field barrier. It therefore begins only inside a
  // finite host frame, after the shell has delivered this title's field quota.
  callGuest(core, kLoadMemoryOverlay, 0x8007AABCu);
  callGuest(core,
            kInitializeMemoryOverlay,
            0x8007AAD0u,
            static_cast<uint32_t>(core.mem_r16s(kMemoryMode)),
            static_cast<uint32_t>(core.mem_r16s(kMemoryVariant)));
  callGuest(core, kInitializeDisplayMode, 0x8007AAE8u, core.mem_r8(kDisplayWidthMode), core.mem_r8(kDisplayHeightMode));

  lucent::info("ts2-boot", "finite FrameDriver completed guest main's overlay initialization");
}

} // namespace ts2
