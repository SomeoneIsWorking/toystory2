#include "frame/frame_boundary.h"

#include "frame/field_call.h"

#include "core.h"
#include "facts/guest_facts.h"
#include "game.h"
#include "game_runtime.h"
#include "runtime/toystory2_context.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

constexpr uint32_t kFieldAccumulator = 0x800A14D4u; // gp+0x7FC, incremented by guest VBlank
constexpr uint32_t kElapsedFields = 0x800A1174u;    // barrier result consumed by update logic
constexpr uint32_t kDeferredDisplayRequest = 0x800A10F8u;
constexpr uint32_t kAlternateUpdateMode = 0x800A10F4u;
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

struct MovieStep {
  uint32_t address;
  uint32_t a0;
  uint32_t a1;
  uint32_t a2;
};
constexpr std::array<MovieStep, 4> kIntroMovieSteps{{
    {kMemoryStatus, 2, 0, 0},
    {kMemoryStatus, 0, 0, 0},
    {kMemoryStatus, 1, 0, 0},
    {kQueueScreen, 0, 0, 1},
}};

// The transition screen retail queues after a selection that is neither a sequence level nor already
// queued; the last level is the finale screen.
std::optional<uint32_t> queuedScreenFor(int selection) {
  const int screen = selection == 11 ? 16 : selection + 1;
  if ((screen % 3) == 0 && selection != 11) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(screen);
}

uint32_t callGuest(Core &core, uint32_t address, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
  const std::array arguments{a0, a1, a2, a3};
  return callGuestToReturn(core, {address, 0x8007A9E8u, arguments, std::nullopt, "frame driver"});
}

} // namespace

FieldCall &FrameCallState::fieldCall() {
  return fieldCall_;
}

int CoreFrameBoundary::displayFieldQuota() const {
  // The resident update and the selection screen both wait two fields per iteration.
  const bool pacedAtTwoFields = state_.outerLoop_.phase == OuterLoopPhase::resident ||
                                state_.outerLoop_.phase == OuterLoopPhase::interactiveSelection;
  return pacedAtTwoFields ? 2 : 1;
}

void CoreFrameBoundary::beginLogicFrame(uint32_t frame) {
  fieldsDelivered_ = 0;
  core_.game->timing.logicFrame = frame;
}

void CoreFrameBoundary::sampleInput() {
  core_.game->pad.serviceFrame();
  // The title's pad decoder reads the guest pad packet that retail republishes every VBlank.
  context(core_).pad.service(core_);
}

void CoreFrameBoundary::tickDisplayField() {
  // Timing::frameTick targets another title; this title's VSync counter is facts::kVSyncQueryCounter.
  ++core_.game->timing.vblank;
  ++fieldsDelivered_;
  core_.mem_w32(facts::kVSyncQueryCounter, core_.game->timing.vblank);
}

void CoreFrameBoundary::serviceDeferredDisplay() {
  if (state_.outerLoop_.phase == OuterLoopPhase::introMovies) {
    return; // the FMV overlay owns the display; 0x80021028 belongs to the resident front end
  }
  if (state_.fieldCall().pending()) {
    // A suspended guest call owns its display loop; a host call here would clobber its registers.
    return;
  }
  // What 0x8003FA68 does at its barrier: publish elapsed fields, clear the accumulator, request 0x80021028.
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

void CoreFrameBoundary::updateResidentGame() {
  stepOuterLoop(state_.outerLoop_, *this);
  context(core_).frameCut.notePassEnded(core_);
}

void CoreFrameBoundary::advanceAudio() {
  core_.game->spu_audio.frame();
}

void CoreFrameBoundary::present(int guestFields) {
  // A native movie's frame is delivered before this present; replay it on top of the guest's layers below.
  const bool movieOnScreen = core_.game->fmv.presenting();
  // On the record path the presenter owns the in-between; there is no temporal decorator.
  core_.game->presentation.commit(&core_, guestFields, nullptr);
  // Packet spans live as long as the record: the OT the next update builds is walked before the next seal.
  core_.rsub.otAttr.beginLogicFrame(core_.game->timing.logicFrame);
  if (movieOnScreen) {
    core_.game->fmv.replayLastFrame(core_);
  }
}

void CoreFrameBoundary::initializeFrontEnd() {
  context(core_).guestMainBoot.finishOverlayInitialization(core_);
  restartColdFrontEnd();
}

void CoreFrameBoundary::restartColdFrontEnd() {
  core_.mem_w32(kPlaybackMode, 0);
  core_.mem_w32(kFrontEndEvent, 0);
  // 0x8007BC74(10,0) decodes the LEVEL00 corpus and needs more than one guest-turn budget, so it runs
  // as a finite initialization transaction.
  const std::array loadArguments{10u, 0u};
  callMemoryDispatcher(loadArguments[0], loadArguments[1], "cold front-end asset load");
  state_.introMovieStep_ = kIntroMovieSteps.size();
  if (core_.mem_r32(kFrontEndEvent) != 9) {
    state_.introMovieStep_ = 0;
  }
}

// Intro: MEMORY status 2, 0, 1, then the queued screen. Each plays a movie inside one guest call, so a
// step delivers one display field (a native player yields a hostSlice instead). A nonzero status ends it.
bool CoreFrameBoundary::stepIntroMovies() {
  while (true) {
    if (!state_.fieldCall().pending()) {
      if (state_.introMovieStep_ >= kIntroMovieSteps.size()) {
        return true;
      }
      const MovieStep &step = kIntroMovieSteps[state_.introMovieStep_];
      const std::array arguments{step.a0, step.a1, step.a2, 0u};
      state_.fieldCall().begin(core_, {step.address, 0x8007A9E8u, arguments, std::nullopt, "front-end movie"});
    }
    const auto progress = state_.fieldCall().advance(core_);
    if (progress == FieldCall::Step::fieldBoundary || progress == FieldCall::Step::hostSlice) {
      return false;
    }
    const bool gated = kIntroMovieSteps[state_.introMovieStep_].address == kMemoryStatus;
    state_.introMovieStep_ =
        gated && state_.fieldCall().result(core_) != 0 ? kIntroMovieSteps.size() : state_.introMovieStep_ + 1;
  }
}

void CoreFrameBoundary::finishColdFrontEnd() {
  core_.mem_w32(0x800A1670u, 1);
  prepareFrontEnd();
}

void CoreFrameBoundary::prepareFrontEnd() {
  // Both decode the front end's asset set (0x80021190 DecompressRAW) over many guest turns.
  callFiniteInitialization(kPrepareMemoryState, "front-end memory-state preparation", kMemoryState);
  callFiniteInitialization(kCommitMemoryState, "front-end memory-state commit", kMemoryState);
  core_.mem_w32(kFrontEndEvent, 0);
  core_.mem_w32(kSelectionActive, 0);
  callGuest(core_, kResetGraphics);
}

// The front-end poll (MEMORY 0x800D92C4) is the title screen's own loop and spans display fields.
std::optional<int> CoreFrameBoundary::pollFrontEndEvent() {
  if (!state_.fieldCall().pending()) {
    const std::array arguments{2u, 0u};
    state_.fieldCall().begin(core_, {kMemoryDispatcher, 0x8007A9E8u, arguments, std::nullopt, "front-end poll"});
    context(core_).yieldAtFieldBarrier = true;
  }
  if (state_.fieldCall().advance(core_) != FieldCall::Step::returned) {
    return std::nullopt;
  }
  context(core_).yieldAtFieldBarrier = false;
  const auto event = static_cast<int>(core_.mem_r32(kFrontEndEvent));
  lucent::info("ts2-frame", "front-end poll returned event {}", event);
  return event;
}

void CoreFrameBoundary::acknowledgeResidentEntry() {
  core_.mem_w32(kFrontEndEvent, 0);
}

void CoreFrameBoundary::finishFrontEndPoll() {
  core_.mem_w32(kFrontEndEvent, static_cast<uint32_t>(-1));
}

bool CoreFrameBoundary::playbackMode() const {
  return core_.mem_r32(kPlaybackMode) != 0;
}

void CoreFrameBoundary::setPlaybackMode(bool enabled) {
  core_.mem_w32(kPlaybackMode, enabled ? 1 : 0);
}

void CoreFrameBoundary::selectPlaybackLevel() {
  static constexpr uint16_t kPhaseSelections[] = {0, 3, 7, 10, 13};
  uint32_t phase = core_.mem_r32(kPlaybackPhase);
  if (phase >= std::size(kPhaseSelections)) {
    lucent::error("ts2-frame", "invalid playback phase {} at 0x{:08X}", phase, kPlaybackPhase);
    std::abort();
  }

  const uint16_t selection = kPhaseSelections[phase];
  lucent::info("ts2-frame",
               "attract rotation step {} selects level index {}, level id 0x{:08X}",
               phase,
               selection,
               levelId(selection));
  core_.mem_w16(kPlaybackLevel, selection);
  phase = (phase + 1) % std::size(kPhaseSelections);
  core_.mem_w32(kPlaybackPhase, phase);
  core_.mem_w32(kLevelId, levelId(selection));
  callGuest(core_, kPlaybackSetup);
  core_.mem_w32(0x800A1640u, static_cast<uint32_t>(-2));
  core_.mem_w32(0x800A163Cu, 0);
  // Same finite asset decoders as prepareFrontEnd.
  callFiniteInitialization(kPrepareMemoryState, "playback memory-state preparation", kMemoryState);
  callFiniteInitialization(kCommitMemoryState, "playback memory-state commit", kMemoryState);
  core_.mem_w16(kPlaybackLevel, selection);
}

bool CoreFrameBoundary::needsInteractiveSelection() const {
  return !playbackMode() && static_cast<int32_t>(core_.mem_r32(kBootCountdown)) < 0;
}

// Selection step 0x8007AD8C: the screen loop 0x80041240, then the queued transition screen; each spans
// display fields. A nonzero screen-loop return means the player backed out (event -1, re-enter the poll).
SelectionProgress CoreFrameBoundary::stepInteractiveSelection() {
  if (!state_.fieldCall().pending()) {
    beginSelectionCall();
  }
  if (state_.fieldCall().advance(core_) != FieldCall::Step::returned) {
    return SelectionProgress::pending;
  }
  context(core_).yieldAtFieldBarrier = false;
  if (state_.selectionCall_ == SelectionCall::queuedScreen) {
    state_.selectionCall_ = SelectionCall::screenLoop;
    return SelectionProgress::chosen;
  }
  if (state_.fieldCall().result(core_) != 0) {
    core_.mem_w32(kFrontEndEvent, static_cast<uint32_t>(-1));
    return SelectionProgress::backToFrontEnd;
  }
  if (!queuedScreenFor(core_.mem_r16s(kPlaybackLevel))) {
    return SelectionProgress::chosen;
  }
  state_.selectionCall_ = SelectionCall::queuedScreen;
  return SelectionProgress::pending;
}

ResidentPreparationProgress CoreFrameBoundary::prepareResident() {
  const uint16_t selection = core_.mem_r16(kPlaybackLevel);
  const uint32_t level = levelId(selection);
  core_.mem_w32(kLevelId, level);
  // 0x8007AE08 passes the selected level to 0x8007BEC4 as well.
  const ResidentPreparationProgress progress =
      state_.residentPreparation_.step(core_, level, static_cast<int>(core_.mem_r32(kPlaybackMode)));
  if (progress != ResidentPreparationProgress::ready) {
    return progress;
  }

  return ResidentPreparationProgress::ready;
}

void CoreFrameBoundary::showMemoryDialog() {
  callMemoryDispatcher(9, 0x80, "memory dialog asset load");
}

// 0x800415E4 (MEMORY CARD) and 0x8004171C (LOAD/SAVE) load assets and then run their own display loop
// on the 0x8003FA68 barrier, so each is a field-spanning call: one display field per step.
void CoreFrameBoundary::beginMemorySelection() {
  beginOverlayCall(kCheckSave, "memory-card selection");
  state_.selectionCall_ = SelectionCall::memoryCardOverlay;
}

void CoreFrameBoundary::beginLoadSaveSelection() {
  beginOverlayCall(kLoadSave, "load-save selection");
  state_.selectionCall_ = SelectionCall::loadSaveOverlay;
}

bool CoreFrameBoundary::stepMemoryScreen() {
  const FieldCall::Step progress = state_.fieldCall().advance(core_);
  if (progress != FieldCall::Step::returned) {
    return false;
  }
  context(core_).yieldAtFieldBarrier = false;
  if (state_.selectionCall_ == SelectionCall::memoryCardOverlay && state_.fieldCall().result(core_) != 0) {
    core_.mem_w32(kSelectionActive, 1);
  }
  state_.selectionCall_ = SelectionCall::screenLoop;
  return true;
}

void CoreFrameBoundary::restartFrontEnd() {
  callMemoryDispatcher(8, 0, "front-end restart asset load");
  core_.mem_w32(kFrontEndEvent, 0);
  prepareFrontEnd();
}

bool CoreFrameBoundary::residentActive() const {
  return core_.mem_r16(kLoopExitReason) == 0 || core_.mem_r16(kExitCountdown) != 0;
}

void CoreFrameBoundary::updateResident() {
  const bool alternate = core_.mem_r32(kAlternateUpdateMode) != 0;
  const std::array noArguments{0u, 0u, 0u, 0u};
  callFiniteGuestToReturn(core_,
                          {residentUpdateAddress(alternate), 0x8007A9E8u, noArguments, std::nullopt, "resident update"},
                          kResidentUpdateSliceLimit);
}

PostResidentTransition CoreFrameBoundary::finishResident() {
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
    return beginSequenceLevel(level);
  }
  return PostResidentTransition::residentSetup;
}

void CoreFrameBoundary::shutdown() {
  callGuest(core_, kShutdownGraphics);
}

void CoreFrameBoundary::beginSelectionCall() {
  if (state_.selectionCall_ == SelectionCall::queuedScreen) {
    const uint32_t screen = *queuedScreenFor(core_.mem_r16s(kPlaybackLevel));
    const std::array arguments{screen, 0u, 0u, 0u};
    state_.fieldCall().begin(core_, {kQueueScreen, 0x8007A9E8u, arguments, std::nullopt, "queued selection screen"});
    return;
  }
  core_.mem_w32(kFrontEndEvent, 0);
  core_.mem_w32(kSelectionActive, 1);
  state_.fieldCall().begin(core_, {kInteractiveSelection, 0x8007A9E8u, {}, std::nullopt, "interactive selection"});
  context(core_).yieldAtFieldBarrier = true;
}

uint32_t CoreFrameBoundary::levelId(uint16_t selection) const {
  return core_.mem_r32(kLevelTable + static_cast<uint32_t>(selection) * 4);
}

bool CoreFrameBoundary::bootCountdownFinished() const {
  return static_cast<int32_t>(core_.mem_r32(kBootCountdown)) >= 0;
}

PostResidentTransition CoreFrameBoundary::finishSequenceMemory() {
  callMemoryDispatcher(5, 0x40, "sequence memory asset load");
  return bootCountdownFinished() ? PostResidentTransition::finished : PostResidentTransition::coldRestart;
}

// Exit reason 1 is the sequence level's end-of-level route; `level % 3 == 0` is the inter-level screen
// route, every other level goes through `pollLevelTransitionEvent`.
PostResidentTransition CoreFrameBoundary::beginSequenceLevel(uint32_t level) {
  if (level % 3 == 0) {
    return finishSequenceScreen(level);
  }
  return PostResidentTransition::levelTransition;
}

PostResidentTransition CoreFrameBoundary::finishSequenceScreen(uint32_t level) {
  const uint16_t selection = core_.mem_r16(kPlaybackLevel);
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
    callMemoryDispatcher(0xB, 0xC0, "finale asset load");
    return PostResidentTransition::coldRestart;
  }
  return PostResidentTransition::residentSetup;
}

// 0x8007BC74(4, 0x40) loads the next asset set and draws the transition screen, so it spans display fields.
std::optional<int> CoreFrameBoundary::pollLevelTransitionEvent() {
  if (!state_.fieldCall().pending()) {
    const std::array arguments{4u, 0x40u};
    state_.fieldCall().begin(core_, {kMemoryDispatcher, 0x8007A9E8u, arguments, std::nullopt, "level transition"});
    context(core_).yieldAtFieldBarrier = true;
  }
  if (state_.fieldCall().advance(core_) != FieldCall::Step::returned) {
    return std::nullopt;
  }
  context(core_).yieldAtFieldBarrier = false;
  callGuest(core_, kCommitSequenceState);
  const uint16_t selection = core_.mem_r16(kPlaybackLevel);
  const uint32_t authoredLevel = core_.mem_r32(kLevelTable + static_cast<uint32_t>(selection) * 4);
  if (core_.mem_r32(0x800A1540u) != core_.mem_r8(0x800C1617u + authoredLevel) &&
      ((callGuest(core_, kScreenStatus) >> 16) & 0xFF) == 0x32) {
    callGuest(core_, kQueueScreen, 0x11, 0x10, 0);
  }
  return bootCountdownFinished() ? 0 : 1;
}

void CoreFrameBoundary::callFiniteInitialization(uint32_t address, std::string_view owner, uint32_t a0) {
  const std::array arguments{a0};
  callFiniteGuestToReturn(
      core_, {address, 0x8007A9E8u, arguments, std::nullopt, owner}, kFiniteInitializationSliceLimit);
}

// Argument 10 only loads assets, so it is a finite call; 4, 2 and 0/1 draw screens and span fields.
void CoreFrameBoundary::callMemoryDispatcher(uint32_t a0, uint32_t a1, std::string_view owner) {
  const std::array arguments{a0, a1, 0u, 0u};
  callFiniteGuestToReturn(
      core_, {kMemoryDispatcher, 0x8007A9E8u, arguments, std::nullopt, owner}, kFiniteInitializationSliceLimit);
}

void CoreFrameBoundary::beginOverlayCall(uint32_t address, const char *owner) {
  if (state_.fieldCall().pending()) {
    return;
  }
  state_.fieldCall().begin(core_, {address, 0x8007A9E8u, {}, std::nullopt, owner});
  context(core_).yieldAtFieldBarrier = true;
}

} // namespace ts2
