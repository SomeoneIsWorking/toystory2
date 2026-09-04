#include "input/native_pad_owner.h"

#include "core.h"
#include "game.h"
#include "guest_execution.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

// RE-06 and RE-18: 0x8003EEF0 passes these buffers to PadInitDirect. The native frame owner writes
// the same standard packet itself, so the linked libpad state machine (which advances on VBlank) is
// neither initialized nor polled.
constexpr uint32_t kPadSlot0 = 0x800CF8A0u;
constexpr uint32_t kPadSlot1 = 0x800CF8C8u;
constexpr uint32_t kDeferredDisplayRequest = 0x800A10F8u;
constexpr uint32_t kPadEnabled = 0x800A109Cu;         // gp+0x3c4
constexpr uint32_t kPadKind = 0x800A0CF4u;            // gp+0x1c
constexpr uint32_t kAnalogPacket = 0x800A10B0u;       // gp+0x3d8
constexpr uint32_t kActuatorCount = 0x800A14A8u;      // gp+0x7d0
constexpr uint32_t kDisconnectedFields = 0x800A1580u; // gp+0x8a8

void writeNativePackets(Core &core) {
  uint8_t packet[4]{};
  core.game->pad.fillBuffer(packet);
  for (uint32_t index = 0; index < 4; ++index) {
    core.mem_w8(kPadSlot0 + index, packet[index]);
    core.mem_w8(kPadSlot1 + index, 0xFFu);
  }
}

void initializeNativePadOverride(Core *core) {
  initializeNativePad(*core);
}

void shutdownNativePadOverride(Core *core) {
  shutdownNativePad(*core);
}

void decodeNativeDigitalPadOverride(Core *core) {
  core->r[2] = decodeNativeDigitalPad(*core);
}

} // namespace

void initializeNativePad(Core &core) {
  // The retail owner waits two fields, starts linked libpad, negotiates vibration/analog mode, then
  // waits five more fields. The native producer deliberately exposes one digital pad and no
  // actuator, so those asynchronous negotiations have no host-side work to complete.
  core.mem_w32(kDeferredDisplayRequest, 0);
  core.mem_w32(kPadKind, 0);
  core.mem_w16(kAnalogPacket, 0);
  core.mem_w32(kActuatorCount, 0);
  core.mem_w16(kDisconnectedFields, 0);
  writeNativePackets(core);
  core.mem_w16(kPadEnabled, 1);
}

void shutdownNativePad(Core &core) {
  // 0x8003EF78 brackets PadStopCom with four VSync calls. The host Pad object is process-owned and
  // stays alive across title display resets; only the guest-visible title gate is shut down.
  core.mem_w32(kDeferredDisplayRequest, 0);
  core.mem_w16(kPadEnabled, 0);
}

uint16_t decodeNativeDigitalPad(Core &core) {
  if (core.mem_r16(kPadEnabled) == 0) {
    return 0;
  }

  if (core.mem_r8(kPadSlot0) == 0xFFu) {
    const int16_t missing = core.mem_r16s(kDisconnectedFields);
    if (missing > 5) {
      core.mem_w32(kPadKind, 3);
      core.mem_w32(kActuatorCount, 0);
      return 0;
    }
    core.mem_w16(kDisconnectedFields, static_cast<uint16_t>(missing + 1));
    return 0;
  }

  core.mem_w16(kDisconnectedFields, 0);
  const uint8_t packetKind = core.mem_r8(kPadSlot0 + 1) & 0xF0u;
  if (packetKind != 0x40u) {
    lucent::error("ts2-pad",
                  "native pad producer emitted unsupported packet kind 0x{:02X} at 0x{:08X}",
                  packetKind,
                  kPadSlot0 + 1);
    std::abort();
  }

  core.mem_w32(kPadKind, 0);
  core.mem_w16(kAnalogPacket, 0);
  return static_cast<uint16_t>(~core.mem_r16(kPadSlot0 + 2));
}

void installNativePadOverrides(Core &core) {
  installResidentOverride(core, 0x8003EEF0u, "pad-init", initializeNativePadOverride);
  installResidentOverride(core, 0x8003EF78u, "pad-shutdown", shutdownNativePadOverride);
  installResidentOverride(core, 0x8003AC58u, "digital-pad-decode", decodeNativeDigitalPadOverride);
}

} // namespace ts2
