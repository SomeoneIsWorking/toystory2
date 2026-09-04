#include "loop/toystory2_frame_driver.h"

#include "boot/guest_main_boot.h"
#include "core.h"
#include "game.h"
#include "game_runtime.h"
#include "guest_execution.h"
#include "loop/outer_loop.h"
#include "loop/resident_frame.h"
#include "loop/resident_preparation.h"
#include "toystory2_context.h"

#include <cstdlib>
#include <iterator>
#include <lucent/log.h>

namespace ts2 {
namespace {

// Exact resident-main facts from 0x8007AEAC..0x8007AEFC and its two-field barrier 0x8003FA68.
constexpr uint32_t kFieldAccumulator = 0x800A14D4u; // gp+0x7FC, incremented by guest VBlank
constexpr uint32_t kElapsedFields = 0x800A1174u;    // barrier result consumed by update logic
constexpr uint32_t kDeferredDisplayRequest = 0x800A10F8u;
constexpr uint32_t kAlternateUpdateMode = 0x800A10F4u;
constexpr uint32_t kLibetcVBlankCount = 0x8009FD54u; // value returned by linked VSync 0x80088628
constexpr uint32_t kDeferredFieldService = 0x80021028u;

constexpr uint32_t kFrontEndEvent = 0x800A1374u; // gp+0x69c
constexpr uint32_t kPlaybackMode = 0x800A120Cu;
constexpr uint32_t kPlaybackLevel = 0x800A1530u;
constexpr uint32_t kPlaybackPhase = 0x800A14D8u;
constexpr uint32_t kLevelId = 0x800A16A8u;
constexpr uint32_t kBootCountdown = 0x800C166Cu;
constexpr uint32_t kMemoryState = 0x800C1608u;
constexpr uint32_t kSelectionActive = 0x800A1420u;
constexpr uint32_t kLevelTable = 0x8009DEF8u;
constexpr uint32_t kLoopExitReason = 0x800A136Eu;
constexpr uint32_t kExitCountdown = 0x800A155Cu;
constexpr uint32_t kReturnToFrontEnd = 0x800A1600u;
constexpr uint32_t kSequenceRemaining = 0x800B2222u;
constexpr uint32_t kSequenceGate = 0x800B221Eu;

constexpr uint32_t kMemoryDispatcher = 0x8007BC74u;
constexpr uint32_t kMemoryStatus = 0x8003EE4Cu;
constexpr uint32_t kQueueScreen = 0x80073408u;
constexpr uint32_t kPrepareMemoryState = 0x80078C34u;
constexpr uint32_t kCommitMemoryState = 0x80078CC4u;
constexpr uint32_t kResetGraphics = 0x8003A774u;
constexpr uint32_t kPlaybackSetup = 0x80079B58u;
constexpr uint32_t kInteractiveSelection = 0x80041240u;
constexpr uint32_t kBeginFade = 0x80077598u;
constexpr uint32_t kCheckSave = 0x800415E4u;
constexpr uint32_t kLoadSave = 0x8004171Cu;
constexpr uint32_t kShutdownGraphics = 0x8003A838u;
constexpr uint32_t kEndResidentDisplay = 0x8003AA74u;
constexpr uint32_t kReleaseResident = 0x8007F010u;
constexpr uint32_t kReleaseMemory = 0x8008214Cu;
constexpr uint32_t kAdvanceSequence = 0x8007BCE4u;
constexpr uint32_t kSetSequenceMode = 0x800412F0u;
constexpr uint32_t kCommitSequenceState = 0x80041818u;
constexpr uint32_t kScreenStatus = 0x80073458u;

uint32_t callGuest(Core &core, uint32_t address, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
  const std::array arguments{a0, a1, a2, a3};
  return callGuestToReturn(core, {address, 0x8007A9E8u, arguments, std::nullopt, "frame driver"});
}

class CoreResidentFrameBoundary final : public ResidentFrameBoundary, public OuterLoopBoundary {
public:
  CoreResidentFrameBoundary(Core &core, OuterLoopState &outerLoop, ResidentPreparation &residentPreparation)
      : core_(core), outerLoop_(outerLoop), residentPreparation_(residentPreparation) {}

  int displayFieldQuota() const override {
    return outerLoop_.phase == OuterLoopPhase::resident ? 2 : 1;
  }

  void beginLogicFrame(uint32_t frame) override {
    fieldsDelivered_ = 0;
    core_.game->timing.logicFrame = frame;
    core_.rsub.otAttr.beginLogicFrame(frame);
  }

  void sampleInput() override {
    core_.game->pad.serviceFrame();
  }

  void tickDisplayField() override {
    // Timing::frameTick mirrors another title's linked-libetc address. Toy Story 2's own VSync body
    // proves its counter at 0x8009FD54, so this title owner advances the shared host count and the
    // correct guest compatibility mirror directly. Presentation pacing advances EmulatedTime once,
    // with the complete two-field quota, at the single commit below.
    ++core_.game->timing.vblank;
    ++fieldsDelivered_;
    core_.mem_w32(kLibetcVBlankCount, core_.game->timing.vblank);
  }

  void serviceDeferredDisplay() override {
    // 0x8003FA68 publishes the number of elapsed fields, clears its accumulator, and asks the next
    // field callback to run 0x80021028. The native owner already controls that boundary, so it
    // performs the same state transition directly instead of spinning for guest VBlank 0x80039D60.
    core_.mem_w32(kElapsedFields, static_cast<uint32_t>(fieldsDelivered_));
    core_.mem_w32(kFieldAccumulator, 0);
    core_.mem_w32(kDeferredDisplayRequest, 1);
    callGuest(core_, kDeferredFieldService);
    if (core_.mem_r32(kDeferredDisplayRequest) != 0) {
      lucent::error("ts2-frame",
                    "deferred field service 0x{:08X} did not acknowledge request 0x{:08X}",
                    kDeferredFieldService,
                    kDeferredDisplayRequest);
      std::abort();
    }
  }

  void updateResidentGame() override {
    stepOuterLoop(outerLoop_, *this);
  }

  void advanceAudio() override {
    core_.game->spu_audio.frame();
  }

  void present(int guestFields) override {
    core_.game->presentation.commit(&core_, guestFields);
  }

  void initializeFrontEnd() override {
    finishGuestMainBoot(core_);
    restartColdFrontEnd();
  }

  void restartColdFrontEnd() override {
    core_.mem_w32(kPlaybackMode, 0);
    core_.mem_w32(kFrontEndEvent, 0);
    callGuest(core_, kMemoryDispatcher, 10, 0);
    if (core_.mem_r32(kFrontEndEvent) != 9 && callGuest(core_, kMemoryStatus, 2, 0) == 0 &&
        callGuest(core_, kMemoryStatus, 0, 0) == 0 && callGuest(core_, kMemoryStatus, 1, 0) == 0) {
      callGuest(core_, kQueueScreen, 0, 0, 1);
    }
    core_.mem_w32(0x800A1670u, 1);
    prepareFrontEnd();
  }

  void prepareFrontEnd() override {
    callGuest(core_, kPrepareMemoryState, kMemoryState);
    callGuest(core_, kCommitMemoryState, kMemoryState);
    core_.mem_w32(kFrontEndEvent, 0);
    core_.mem_w32(kSelectionActive, 0);
    callGuest(core_, kResetGraphics);
  }

  int pollFrontEndEvent() override {
    callGuest(core_, kMemoryDispatcher, 2, 0);
    return static_cast<int>(core_.mem_r32(kFrontEndEvent));
  }

  void acknowledgeResidentEntry() override {
    core_.mem_w32(kFrontEndEvent, 0);
  }

  void finishFrontEndPoll() override {
    core_.mem_w32(kFrontEndEvent, static_cast<uint32_t>(-1));
  }

  bool playbackMode() const override {
    return core_.mem_r32(kPlaybackMode) != 0;
  }

  void setPlaybackMode(bool enabled) override {
    core_.mem_w32(kPlaybackMode, enabled ? 1 : 0);
  }

  void selectPlaybackLevel() override {
    static constexpr uint16_t kPhaseSelections[] = {0, 3, 7, 10, 13};
    uint32_t phase = core_.mem_r32(kPlaybackPhase);
    if (phase >= std::size(kPhaseSelections)) {
      lucent::error("ts2-frame", "invalid playback phase {} at 0x{:08X}", phase, kPlaybackPhase);
      std::abort();
    }

    const uint16_t selection = kPhaseSelections[phase];
    core_.mem_w16(kPlaybackLevel, selection);
    phase = (phase + 1) % std::size(kPhaseSelections);
    core_.mem_w32(kPlaybackPhase, phase);
    core_.mem_w32(kLevelId, levelId(selection));
    callGuest(core_, kPlaybackSetup);
    core_.mem_w32(0x800A1640u, static_cast<uint32_t>(-2));
    core_.mem_w32(0x800A163Cu, 0);
    callGuest(core_, kPrepareMemoryState, kMemoryState);
    callGuest(core_, kCommitMemoryState, kMemoryState);
    core_.mem_w16(kPlaybackLevel, selection);
  }

  bool needsInteractiveSelection() const override {
    return !playbackMode() && static_cast<int32_t>(core_.mem_r32(kBootCountdown)) < 0;
  }

  bool stepInteractiveSelection() override {
    core_.mem_w32(kFrontEndEvent, 0);
    core_.mem_w32(kSelectionActive, 1);
    if (callGuest(core_, kInteractiveSelection) != 0) {
      return true;
    }

    const int selection = core_.mem_r16s(kPlaybackLevel);
    int screen = selection + 1;
    if ((screen % 3) != 0 || selection == 11) {
      if (selection == 11) {
        screen = 16;
      }
      callGuest(core_, kQueueScreen, static_cast<uint32_t>(screen), 0, 0);
    }
    return false;
  }

  ResidentPreparationProgress prepareResident() override {
    const uint16_t selection = core_.mem_r16(kPlaybackLevel);
    const uint32_t level = levelId(selection);
    core_.mem_w32(kLevelId, level);
    // Retail 0x8007AE08 stores and passes the same selected level to 0x8007BEC4. The finite owner
    // receives that live argument directly; dropping it forces absent LEVEL00/LEVEL.DAT retries.
    const ResidentPreparationProgress progress =
        residentPreparation_.step(core_, level, static_cast<int>(core_.mem_r32(kPlaybackMode)));
    if (progress != ResidentPreparationProgress::ready) {
      return progress;
    }

    core_.mem_w32(0x800A1480u, 0);
    core_.mem_w32(0x800A11E4u, 0);
    core_.mem_w32(kAlternateUpdateMode, 0);
    core_.mem_w32(0x800A11A4u, 0);
    core_.mem_w32(0x800A15BCu, 0);
    core_.mem_w32(0x800A1324u, 0);
    core_.mem_w16(0x800A1638u, 0);
    core_.mem_w16(0x800A163Au, 0);
    core_.mem_w32(kElapsedFields, 0);
    core_.mem_w16(0x800A136Eu, 0);
    core_.mem_w16(0x800A155Cu, 90);
    const int32_t bootCountdown = static_cast<int32_t>(core_.mem_r32(kBootCountdown));
    core_.mem_w32(0x800A1430u, static_cast<uint32_t>(bootCountdown * 2));
    callGuest(core_, kBeginFade, 128, 128, 128, 6);
    context(core_).camera.reset();
    context(core_).scene.reset();
    return ResidentPreparationProgress::ready;
  }

  void showMemoryDialog() override {
    callGuest(core_, kMemoryDispatcher, 9, 0x80);
  }

  void checkSaveSelection() override {
    if (callGuest(core_, kCheckSave) != 0) {
      core_.mem_w32(kSelectionActive, 1);
    }
  }

  void loadSaveSelection() override {
    callGuest(core_, kLoadSave);
  }

  void restartFrontEnd() override {
    callGuest(core_, kMemoryDispatcher, 8, 0);
    core_.mem_w32(kFrontEndEvent, 0);
    prepareFrontEnd();
  }

  bool residentActive() const override {
    return core_.mem_r16(kLoopExitReason) == 0 || core_.mem_r16(kExitCountdown) != 0;
  }

  void updateResident() override {
    const bool alternate = core_.mem_r32(kAlternateUpdateMode) != 0;
    context(core_).scene.beginFrame();
    callGuest(core_, residentUpdateAddress(alternate));
    // Both resident update owners call camera producer 0x8002C848 before the later scene root
    // 0x8002A070. Capture its authored input after the update so future native producers and temporal
    // presentation share one previous/current source rather than re-reading mutable guest RAM.
    context(core_).camera.capture(core_);
    context(core_).scene.finishFrame();
  }

  PostResidentTransition finishResident() override {
    callGuest(core_, kEndResidentDisplay, 0);
    callGuest(core_, kReleaseResident);

    if (core_.mem_r16(kLoopExitReason) != 2) {
      callGuest(core_, kReleaseMemory);
    }
    if (core_.mem_r32(kReturnToFrontEnd) != 0) {
      if (core_.mem_r16(kLoopExitReason) == 2) {
        callGuest(core_, kReleaseMemory);
      }
      return PostResidentTransition::frontEndSetup;
    }

    const uint16_t reason = core_.mem_r16(kLoopExitReason);
    if (reason == 2) {
      if (core_.mem_r16(kSequenceRemaining) == 0) {
        callGuest(core_, kReleaseMemory);
        return finishSequenceMemory();
      }
      callGuest(core_, kAdvanceSequence);
      return PostResidentTransition::residentSetup;
    }

    if (reason == 3 || reason == 4) {
      if (bootCountdownFinished()) {
        return PostResidentTransition::finished;
      }
      return reason == 3 ? PostResidentTransition::coldRestart : PostResidentTransition::frontEndSetup;
    }

    if (reason == 5 && core_.mem_r16s(kSequenceGate) < 0) {
      const int remaining = core_.mem_r16s(kSequenceRemaining);
      if (remaining == 0) {
        return finishSequenceMemory();
      }
      core_.mem_w16(kSequenceRemaining, static_cast<uint16_t>(remaining - 1));
    }
    if (bootCountdownFinished()) {
      return PostResidentTransition::finished;
    }

    const uint32_t level = core_.mem_r32(kLevelId);
    if (reason == 5 && level % 3 != 0) {
      core_.mem_w16(kLoopExitReason, 1);
    }
    if (core_.mem_r16(kLoopExitReason) == 1) {
      return finishSequenceLevel(level);
    }
    return PostResidentTransition::residentSetup;
  }

  void shutdown() override {
    callGuest(core_, kShutdownGraphics);
  }

private:
  uint32_t levelId(uint16_t selection) const {
    return core_.mem_r32(kLevelTable + static_cast<uint32_t>(selection) * 4);
  }

  bool bootCountdownFinished() const {
    return static_cast<int32_t>(core_.mem_r32(kBootCountdown)) >= 0;
  }

  PostResidentTransition finishSequenceMemory() {
    callGuest(core_, kMemoryDispatcher, 5, 0x40);
    return bootCountdownFinished() ? PostResidentTransition::finished : PostResidentTransition::coldRestart;
  }

  PostResidentTransition finishSequenceLevel(uint32_t level) {
    const uint16_t selection = core_.mem_r16(kPlaybackLevel);
    if (level % 3 == 0) {
      callGuest(core_, kSetSequenceMode, 4);
      const uint32_t completionFlag = 0x800C1628u + selection;
      const uint8_t wasComplete = core_.mem_r8(completionFlag);
      core_.mem_w8(completionFlag, 1);
      if (level == 0xF) {
        core_.mem_w8(0x800C1639u, 1);
      }
      callGuest(core_, kCommitSequenceState);
      if (wasComplete == 0) {
        callGuest(core_, kQueueScreen, selection + 1, 0x1E, 1);
      }
      if (level == 0xF) {
        callGuest(core_, kQueueScreen, 0x12, 0x1F, 1);
        callGuest(core_, kMemoryDispatcher, 0xB, 0xC0);
        return PostResidentTransition::coldRestart;
      }
      return PostResidentTransition::residentSetup;
    }

    callGuest(core_, kMemoryDispatcher, 4, 0x40);
    if (bootCountdownFinished()) {
      return PostResidentTransition::finished;
    }
    callGuest(core_, kCommitSequenceState);
    const uint32_t authoredLevel = core_.mem_r32(kLevelTable + static_cast<uint32_t>(selection) * 4);
    if (core_.mem_r32(0x800A1540u) != core_.mem_r8(0x800C1617u + authoredLevel) &&
        ((callGuest(core_, kScreenStatus) >> 16) & 0xFF) == 0x32) {
      callGuest(core_, kQueueScreen, 0x11, 0x10, 0);
    }
    return PostResidentTransition::residentSetup;
  }

  Core &core_;
  OuterLoopState &outerLoop_;
  ResidentPreparation &residentPreparation_;
  int fieldsDelivered_ = 0;
};

class ToyStory2FrameDriver final : public FrameDriver {
public:
  void stepFrame(Core &core, uint32_t frame) override {
    CoreResidentFrameBoundary boundary(core, outerLoop_, residentPreparation_);
    stepResidentFrame(boundary, frame);
  }

private:
  OuterLoopState outerLoop_;
  ResidentPreparation residentPreparation_;
};

} // namespace

std::unique_ptr<FrameDriver> createFrameDriver(Game &) {
  return std::make_unique<ToyStory2FrameDriver>();
}

} // namespace ts2
