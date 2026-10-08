#include "boot/graphics_sync.h"

#include "boot/level_start_presentation.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "execution_control.h"
#include "input/pad_owner.h"
#include "native_dispatch.h"
#include "runtime/toystory2_context.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

constexpr uint32_t kGraphicsBufferA = 0x801BBD28u;
constexpr uint32_t kGraphicsBufferB = 0x801DD21Cu;
constexpr uint32_t kCurrentGraphicsBuffer = 0x800A1468u; // gp+0x790
constexpr uint32_t kOrderingTable = 0x800A1608u;         // gp+0x930
constexpr uint32_t kPrimitivePool = 0x800A10BCu;         // gp+0x3e4
constexpr uint32_t kElapsedFields = 0x800A1174u;
constexpr uint32_t kFieldAccumulator = 0x800A14D4u;
constexpr uint32_t kDeferredDisplayRequest = 0x800A10F8u;

// The caller-saved registers the guest expects back from its own body must be handed back untouched.
class GuestRegisters {
public:
  explicit GuestRegisters(Core &core) : core_(core) {
    std::copy(std::begin(core.r), std::end(core.r), saved_.begin());
  }
  ~GuestRegisters() {
    std::copy(saved_.begin(), saved_.end(), std::begin(core_.r));
  }
  GuestRegisters(const GuestRegisters &) = delete;
  GuestRegisters &operator=(const GuestRegisters &) = delete;

private:
  Core &core_;
  std::array<uint32_t, 32> saved_{};
};

void dispatchGuest(Core &core,
                   uint32_t address,
                   uint32_t a0 = 0,
                   uint32_t a1 = 0,
                   uint32_t a2 = 0,
                   uint32_t a3 = 0,
                   bool hasStackArgument = false,
                   uint32_t stackArgument = 0) {
  const std::array arguments{a0, a1, a2, a3};
  callGuestToReturn(core,
                    {address,
                     0x8003A218u,
                     arguments,
                     hasStackArgument ? std::optional<uint32_t>{stackArgument} : std::nullopt,
                     "native graphics owner"});
}

void selectGraphicsBuffer(Core &core, uint32_t firstChoice) {
  uint32_t selected = firstChoice;
  if (core.mem_r32(kCurrentGraphicsBuffer) == kGraphicsBufferA) {
    selected = kGraphicsBufferB;
  }
  core.mem_w32(kOrderingTable, selected + 0x1924);
  core.mem_w32(kCurrentGraphicsBuffer, selected);
  core.mem_w32(kPrimitivePool, selected + 0x2C4);
  core.mem_w16(0x800A1E64u, 0xFFFF);
  dispatchGuest(core, 0x80086320u, selected + 0x270);
  dispatchGuest(core, 0x80086054u, selected + 0x18A0);
  dispatchGuest(core, 0x80085A54u, 0);
}

void initializeGraphicsWithoutGuestVSync(Core *core) {
  const GuestRegisters registers(*core);
  core->r[29] -= 64;
  const uint32_t rect = core->r[29] + 24;

  core->mem_w16(rect + 0, 0);
  core->mem_w16(rect + 2, 0x100);
  core->mem_w16(rect + 4, 0x280);
  core->mem_w16(rect + 6, 0xFF);
  dispatchGuest(*core, 0x80085BE8u, rect, 0, 0, 0);
  core->mem_w16(rect + 0, 0);
  core->mem_w16(rect + 2, 0x1FF);
  core->mem_w16(rect + 4, 0x280);
  core->mem_w16(rect + 6, 1);
  dispatchGuest(*core, 0x80085BE8u, rect, 0, 0, 0);
  dispatchGuest(*core, 0x80085A54u, 0);

  dispatchGuest(*core, 0x80084DACu, kGraphicsBufferA, 0x140, 0x100, 0x140, true, 0xF0);
  dispatchGuest(*core, 0x80084E64u, kGraphicsBufferA + 0x270, 0, 0x100, 0x140, true, 0xF0);
  core->mem_w8(kGraphicsBufferA + 0x16, 1);
  core->mem_w8(kGraphicsBufferA + 0x17, 0);
  core->mem_w8(kGraphicsBufferA + 0x18, 1);
  core->mem_w8(kGraphicsBufferA + 0x19, 0x40);
  core->mem_w8(kGraphicsBufferA + 0x1A, 0x80);
  core->mem_w8(kGraphicsBufferA + 0x1B, 0x80);
  dispatchGuest(*core, 0x80085F5Cu, kGraphicsBufferA + 0x2B4, 0x57C);

  dispatchGuest(*core, 0x80084DACu, kGraphicsBufferB, 0, 0x100, 0x140, true, 0xF0);
  dispatchGuest(*core, 0x80084E64u, kGraphicsBufferB + 0x270, 0x140, 0x100, 0x140, true, 0xF0);
  core->mem_w8(kGraphicsBufferB + 0x16, 1);
  core->mem_w8(kGraphicsBufferB + 0x17, 0);
  core->mem_w8(kGraphicsBufferB + 0x18, 1);
  core->mem_w8(kGraphicsBufferB + 0x19, 0x40);
  core->mem_w8(kGraphicsBufferB + 0x1A, 0x80);
  core->mem_w8(kGraphicsBufferB + 0x1B, 0x80);
  dispatchGuest(*core, 0x80085F5Cu, kGraphicsBufferB + 0x2B4, 0x57C);

  core->mem_w16(kGraphicsBufferA + 0x27E, 0xF0);
  core->mem_w16(kGraphicsBufferB + 0x27E, 0xF0);
  const uint16_t displayX = static_cast<uint16_t>(core->mem_r32(0x800A11C4u));
  const uint16_t displayY = static_cast<uint16_t>(core->mem_r32(0x800A11C8u));
  core->mem_w16(kGraphicsBufferA + 0x278, displayX);
  core->mem_w16(kGraphicsBufferA + 0x27A, displayY);
  core->mem_w16(kGraphicsBufferB + 0x278, displayX);
  core->mem_w16(kGraphicsBufferB + 0x27A, displayY);

  // Retail waits two fields before each swap; the host frame owner supplies them.
  selectGraphicsBuffer(*core, kGraphicsBufferA);
  selectGraphicsBuffer(*core, kGraphicsBufferA);

  core->mem_w16(rect + 0, 0);
  core->mem_w16(rect + 2, 0);
  core->mem_w16(rect + 4, 0x100);
  core->mem_w16(rect + 6, 0x100);
  const uint32_t texturePage = static_cast<uint32_t>(core->mem_r16s(0x800CD3D0u));
  dispatchGuest(*core, 0x80086960u, kGraphicsBufferA + 0x290, 1, 1, texturePage, true, rect);
  dispatchGuest(*core, 0x80086960u, kGraphicsBufferB + 0x290, 1, 1, texturePage, true, rect);

  const bool bufferASelected = core->mem_r32(kCurrentGraphicsBuffer) == kGraphicsBufferA;
  core->mem_w32(0x800A11D0u, core->mem_r32(bufferASelected ? 0x800A114Cu : 0x800A1150u));
  core->mem_w16(0x800A12E8u, 0);
  core->mem_w32(0x800A13F0u, bufferASelected ? 0x801C5A20u : 0x801E6F14u);
  core->mem_w32(0x800A142Cu, bufferASelected ? 0x801E6F14u : 0x801C5A20u);
  core->mem_w32(0x800A14B4u, 0);
}

void completeOwnedFieldBarrier(Core *core) {
  // Publish 0x8003FA68's RAM postcondition without dispatching guest VBlank or presenting.
  const uint32_t requested = core->r[4];
  const GuestRegisters registers(*core);
  const bool yieldsToHost = context(*core).yieldAtFieldBarrier;
  const uint32_t elapsed = yieldsToHost ? requested : std::min(core->mem_r32(kElapsedFields), 4u);
  if (elapsed < requested) {
    lucent::error(
        "ts2-frame", "field barrier requested {} fields after the native owner supplied only {}", requested, elapsed);
    std::abort();
  }
  core->mem_w32(kElapsedFields, elapsed);
  core->mem_w32(kFieldAccumulator, 0);
  core->mem_w32(kDeferredDisplayRequest, 1);
  dispatchGuest(*core, 0x80021028u);
  if (core->mem_r32(kDeferredDisplayRequest) != 0) {
    lucent::error("ts2-frame", "native field barrier did not receive deferred-display acknowledgement");
    std::abort();
  }
  if (yieldsToHost) {
    psx::cpu::requestExecutionExit(*core, psx::cpu::ExecutionExitReason::FrameBoundary);
  }
}

// Retail 0x80039D9C, driven through the same libgpu leaves but without its guest VSync waits.
void initializeResidentGraphicsWithoutGuestVSync(Core &core) {
  const GuestRegisters registers(core);
  core.r[29] -= 72;
  const uint32_t rect = core.r[29] + 24;
  const auto setRect = [&](uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    core.mem_w16(rect + 0, x);
    core.mem_w16(rect + 2, y);
    core.mem_w16(rect + 4, width);
    core.mem_w16(rect + 6, height);
  };
  // 512 columns of VRAM in one of the two halves.
  constexpr uint32_t drawWidth = 0x200;

  setRect(0, 0x100, 0x3FF, 0xFF);
  dispatchGuest(core, 0x80085BE8u, rect, 0, 0, 0);
  setRect(0, 0x1FF, 0x3FF, 1);
  dispatchGuest(core, 0x80085BE8u, rect, 0, 0, 0);
  setRect(0x3FF, 0x100, 1, 0xFF);
  dispatchGuest(core, 0x80085BE8u, rect, 0, 0, 0);
  setRect(0x3FF, 0x1FF, 1, 1);
  dispatchGuest(core, 0x80085BE8u, rect, 0, 0, 0);
  dispatchGuest(core, 0x80085A54u, 0);

  dispatchGuest(core, 0x80084DACu, kGraphicsBufferA, 0x200, 0x100, drawWidth, true, 0xF0);
  core.mem_w8(kGraphicsBufferA + 0x16, 1);
  core.mem_w8(kGraphicsBufferA + 0x17, 0);
  core.mem_w8(kGraphicsBufferA + 0x18, 0);
  core.mem_w8(kGraphicsBufferA + 0x19, 0x40);
  core.mem_w8(kGraphicsBufferA + 0x1A, 0x80);
  core.mem_w8(kGraphicsBufferA + 0x1B, 0x80);
  dispatchGuest(core, 0x80084DACu, kGraphicsBufferA + 0x5C, 0x200, 0, drawWidth, true, 0x100);
  dispatchGuest(core, 0x80084E64u, kGraphicsBufferA + 0x270, 0, 0x100, 0x200, true, 0xF0);
  dispatchGuest(core, 0x80084EA0u, 2, 1, 0x300, 0);
  core.mem_w16(kGraphicsBufferA + 0x70, static_cast<uint16_t>(core.r[2]));
  dispatchGuest(core, 0x800869B8u, kGraphicsBufferA + 0x170, kGraphicsBufferA);
  dispatchGuest(core, 0x800869B8u, kGraphicsBufferA + 0x1B0, kGraphicsBufferA + 0x5C);
  core.mem_w8(kGraphicsBufferA + 0x18, 1);
  dispatchGuest(core, 0x80085F5Cu, kGraphicsBufferA + 0x2B4, 0x57C);

  dispatchGuest(core, 0x80084DACu, kGraphicsBufferB, 0, 0x100, drawWidth, true, 0xF0);
  core.mem_w8(kGraphicsBufferB + 0x16, 1);
  core.mem_w8(kGraphicsBufferB + 0x17, 0);
  core.mem_w8(kGraphicsBufferB + 0x18, 0);
  core.mem_w8(kGraphicsBufferB + 0x19, 0x40);
  core.mem_w8(kGraphicsBufferB + 0x1A, 0x80);
  core.mem_w8(kGraphicsBufferB + 0x1B, 0x80);
  dispatchGuest(core, 0x80084E64u, kGraphicsBufferB + 0x270, 0x200, 0x100, 0x200, true, 0xF0);
  dispatchGuest(core, 0x80084DACu, kGraphicsBufferB + 0x5C, 0x200, 0, drawWidth, true, 0x100);
  dispatchGuest(core, 0x80084EA0u, 2, 1, 0x300, 0);
  core.mem_w16(kGraphicsBufferB + 0x70, static_cast<uint16_t>(core.r[2]));
  dispatchGuest(core, 0x800869B8u, kGraphicsBufferB + 0x170, kGraphicsBufferB);
  dispatchGuest(core, 0x800869B8u, kGraphicsBufferB + 0x1B0, kGraphicsBufferB + 0x5C);
  core.mem_w8(kGraphicsBufferB + 0x18, 1);
  dispatchGuest(core, 0x80085F5Cu, kGraphicsBufferB + 0x2B4, 0x57C);

  const uint16_t displayX = static_cast<uint16_t>(core.mem_r32(0x800A11C4u));
  const uint16_t displayY = static_cast<uint16_t>(core.mem_r32(0x800A11C8u));
  for (const uint32_t buffer : {kGraphicsBufferA, kGraphicsBufferB}) {
    core.mem_w16(buffer + 0x278, displayX);
    core.mem_w16(buffer + 0x27A, displayY);
    core.mem_w16(buffer + 0x27E, 0xF0);
  }

  // Only the two buffer-state transitions of retail's two pre-swap waits remain.
  selectGraphicsBuffer(core, kGraphicsBufferA);
  selectGraphicsBuffer(core, kGraphicsBufferA);

  setRect(0, 0, 0x100, 0x100);
  const uint32_t texturePage = static_cast<uint32_t>(core.mem_r16s(0x800CD3D0u));
  dispatchGuest(core, 0x80086960u, kGraphicsBufferA + 0x290, 1, 1, texturePage, true, rect);
  dispatchGuest(core, 0x80086960u, kGraphicsBufferB + 0x290, 1, 1, texturePage, true, rect);

  const bool bufferASelected = core.mem_r32(kCurrentGraphicsBuffer) == kGraphicsBufferA;
  core.mem_w16(0x800A12E8u, 0);
  core.mem_w32(0x800A11D0u, core.mem_r32(bufferASelected ? 0x800A114Cu : 0x800A1150u));
  core.mem_w32(0x800A13F0u, bufferASelected ? 0x801C5A20u : 0x801E6F14u);
  core.mem_w32(0x800A142Cu, bufferASelected ? 0x801E6F14u : 0x801C5A20u);
  core.mem_w32(0x800A14B4u, 1);
}

void initializeResidentGraphicsOverride(Core *core) {
  initializeResidentGraphicsWithoutGuestVSync(*core);
}

void shutdownGraphicsWithoutGuestVSync(Core *core) {
  const GuestRegisters registers(*core);

  // Only VSync is omitted from retail 0x8003A838; the four state transitions are the guest's own.
  dispatchGuest(*core, 0x80085A54u, 0);
  dispatchGuest(*core, 0x80085A54u, 0);
  dispatchGuest(*core, 0x80088920u);
  context(*core).pad.shutdown(*core);
  dispatchGuest(*core, 0x80085594u, 3);
}

} // namespace

void GraphicsSync::install(Core &core) {
  psx::cpu::installNativeOverride(core, 0x8003A218u, "graphics-init", initializeGraphicsWithoutGuestVSync);
  psx::cpu::installNativeOverride(core, 0x80039D9Cu, "resident-graphics-init", initializeResidentGraphicsOverride);
  psx::cpu::installNativeOverride(core, 0x8003A838u, "graphics-shutdown", shutdownGraphicsWithoutGuestVSync);
  psx::cpu::installNativeOverride(core, 0x8003FA68u, "field-barrier", completeOwnedFieldBarrier);
}

} // namespace ts2
