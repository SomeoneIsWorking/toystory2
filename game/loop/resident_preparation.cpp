#include "loop/resident_preparation.h"

#include "boot/native_sync_overrides.h"
#include "core.h"
#include "guest_execution.h"

#include <algorithm>
#include <array>

namespace ts2 {
namespace {

constexpr uint32_t kLevelId = 0x800A16A8u;
constexpr uint32_t kPlaybackMode = 0x800A120Cu;
constexpr uint32_t kBootCountdown = 0x800C166Cu;
constexpr uint32_t kElapsedFields = 0x800A1174u;
constexpr uint32_t kTransitionFlags = 0x800A1480u;
constexpr uint32_t kPreviousTransitionFlags = 0x800A11E4u;

uint32_t callGuest(Core &core, uint32_t address, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
  const std::array arguments{a0, a1, a2, a3};
  return callGuestToReturn(core, {address, 0x8007BEC4u, arguments, std::nullopt, "resident preparation"});
}

void zeroHalfwords(Core &core, uint32_t address, unsigned count) {
  for (unsigned index = 0; index < count; ++index) {
    core.mem_w16(address + index * 2, 0);
  }
}

} // namespace

ResidentPreparationProgress ResidentPreparation::step(Core &core, uint32_t level, int playbackMode) {
  std::array<uint32_t, 32> savedRegisters{};
  std::copy(std::begin(core.r), std::end(core.r), savedRegisters.begin());

  ResidentPreparationProgress progress = ResidentPreparationProgress::pending;
  if (phase_ == Phase::begin) {
    begin(core, level, playbackMode);
  } else {
    progress = stepTransition(core);
  }

  std::copy(savedRegisters.begin(), savedRegisters.end(), std::begin(core.r));
  return progress;
}

void ResidentPreparation::begin(Core &core, uint32_t level, int playbackMode) {
  level_ = level;
  playbackMode_ = playbackMode;
  core.mem_w16(0x800A1340u, 0);
  core.mem_w16(0x800A15F4u, 0);
  core.mem_w16(0x800A14D0u, 0);

  callGuest(core, 0x8003A218u);
  callGuest(core, 0x8007C278u, level_, static_cast<uint32_t>(playbackMode_));
  callGuest(core, 0x8003D88Cu, level_);

  const int bootCountdown = static_cast<int>(core.mem_r32(kBootCountdown));
  bootFieldsRemaining_ = bootCountdown >= 0 ? bootCountdown * 2 : -1;
  fadeFieldsRemaining_ = 0x1C;
  cycle_ = 0;
  interrupted_ = false;
  core.mem_w32(0x800A0CE0u, 0);
  core.mem_w32(0x800A0CE4u, 0);

  fadeActive_ = playbackMode_ != 0 && playbackMode_ != 0x7B;
  fadePosition_ = fadeActive_ ? 0x400 : 0;
  if (fadeActive_) {
    callGuest(core, 0x80077598u, 0, 0, 0, 0xC);
  }
  phase_ = Phase::transition;
}

ResidentPreparationProgress ResidentPreparation::stepTransition(Core &core) {
  // Retail 0x8007C344 requests one field per iteration. The title frame shell supplied that field
  // before this operation, so consume exactly one authored iteration without entering 0x8003FA68.
  core.mem_w32(kElapsedFields, 1);
  if (fadeActive_ && fadeFieldsRemaining_ > 0) {
    --fadeFieldsRemaining_;
  }
  if (bootFieldsRemaining_ > 0 && --bootFieldsRemaining_ == 0) {
    core.mem_w16(kTransitionFlags, static_cast<uint16_t>(core.mem_r16(kTransitionFlags) | 1u));
  }

  const uint16_t flags = core.mem_r16(kTransitionFlags);
  if ((flags & 0x4000u) != 0 && (core.mem_r16(kPreviousTransitionFlags) & 0x4000u) == 0 && !fadeActive_) {
    callGuest(core, 0x80077598u, 0, 0, 0, 0xC);
    fadeActive_ = true;
    if (core.mem_r16s(0x800A1210u) == 0) {
      callGuest(core, 0x8007F4F8u, 0, 0x1200, 0x50, 0x50);
    } else {
      callGuest(core, 0x80072C7Cu, 7, 0);
    }
  }
  if ((flags & 1u) != 0 && !fadeActive_ && static_cast<int32_t>(core.mem_r32(kBootCountdown)) >= 0) {
    callGuest(core, 0x80077598u, 0, 0, 0, 0xC);
    interrupted_ = true;
    fadeActive_ = true;
  }

  callGuest(core, 0x800775CCu);
  cycle_ = (cycle_ + 1u) & 0x3Fu;
  if (cycle_ > 0x1Eu) {
    callGuest(core, 0x80045C2Cu, 0x20, 0xC0, 0x80, 0);
  }
  if (fadePosition_ < 0x400 && playbackMode_ != 0x7B) {
    fadePosition_ += 0x20;
    const uint16_t authored = core.mem_r16(0x80098648u + static_cast<uint32_t>(fadePosition_) * 2u);
    const int32_t authoredHigh = static_cast<int32_t>(static_cast<uint32_t>(authored) << 16) >> 24;
    callGuest(core, 0x80045C2Cu, 0x20, static_cast<uint32_t>(0x100 - authoredHigh), 0x80, 1);
  }
  callGuest(core, 0x80046A88u);

  if (fadeFieldsRemaining_ != 0) {
    return ResidentPreparationProgress::pending;
  }

  const bool finished = finish(core);
  phase_ = Phase::begin;
  return finished ? ResidentPreparationProgress::finished : ResidentPreparationProgress::ready;
}

bool ResidentPreparation::finish(Core &core) {
  initializeResidentGraphicsWithoutGuestVSync(core);
  core.mem_w32(0x800A0CE0u, 0xFFFF8000u);
  core.mem_w32(0x800A0CE4u, 0xFFFF8000u);

  callGuest(core, 0x8007C5F8u);
  core.mem_w32(0x800A1208u, 0x800206ECu);
  core.mem_w16(0x800A136Eu, 0);
  core.mem_w16(0x800A155Cu, 0);
  core.mem_w16(0x800A10F4u, 0);
  core.mem_w16(0x800A11A4u, 0);
  core.mem_w16(0x800A1564u, 0);
  core.mem_w16(0x800A1624u, 0);
  core.mem_w16(0x800A148Cu, 0);
  core.mem_w16(0x800A1494u, 0);
  core.mem_w16(0x800A1300u, 0);
  core.mem_w8(0x800A12F4u, 0);
  core.mem_w8(0x800A15BEu, 0);
  core.mem_w8(0x800A149Cu, 0);
  core.mem_w8(0x800A15BFu, 0);
  core.mem_w32(0x800A1654u, 0x36);
  callGuest(core, 0x80082E7Cu, 0x800B2358u, 0, 0xC);
  callGuest(core, 0x80082E7Cu, 0x800A4400u, 0, 0xC);
  callGuest(core, 0x80082E7Cu, 0x800B2278u, 0, 0xA0);
  core.mem_w16(0x800C2AC8u, 0x400);
  core.mem_w16(0x800C2ACAu, 0x400);
  core.mem_w16(0x800C2ACCu, 0x400);
  zeroHalfwords(core, 0x800C2ACEu, 8);
  core.mem_w16(0x800CD400u, 0xB4);
  core.mem_w16(0x800CD402u, 0xB4);
  core.mem_w16(0x800CD404u, 0xB4);
  zeroHalfwords(core, 0x800CD406u, 8);
  callGuest(core, 0x800492A8u, 0x800B2188u, level_);
  callGuest(core, 0x800740FCu);
  callGuest(core, 0x80065CD0u, 0x800BA048u, 0x800B2188u);
  callGuest(core, 0x8006D668u);
  callGuest(core, 0x8007C968u);
  core.mem_w32(0x800A1370u, 900);
  core.mem_w32(0x800A1464u, 0xCE);
  core.mem_w32(0x800A4298u, 0);
  core.mem_w32(0x800A11F8u, 0);
  core.mem_w32(0x800A112Cu, 0);
  core.mem_w32(0x800A1104u, 0xFFFFFFFFu);
  core.mem_w8(0x800A1390u, 0x80);
  core.mem_w8(0x800A1358u, 0x80);
  core.mem_w8(0x800A1342u, 0x80);
  core.mem_w32(0x800A10C8u, 0);
  core.mem_w32(0x800A1360u, 0);
  callGuest(core, 0x80077308u, level_);
  callGuest(core, 0x80077D44u);
  core.mem_w32(0x800A1298u, core.mem_r8(0x800C160Au));
  zeroHalfwords(core, 0x800C94F0u, 10);
  core.mem_w32(0x800A1540u, core.mem_r8(0x800C1617u + level_));
  core.mem_w32(0x800A1660u, 0);
  core.mem_w32(0x800A1418u, 0);
  core.mem_w32(0x800A12CCu, 0);
  core.mem_w32(0x800A1164u, 0xFFFFFFFFu);
  const int selection = core.mem_r16s(0x800A1530u);
  if (level_ - 1u < 0xFu && selection < 0xF && level_ % 3u != 0) {
    callGuest(core, 0x80082D20u);
  }
  return interrupted_;
}

} // namespace ts2
