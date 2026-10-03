// frame_boundary.cpp — the measured guest operations the frame turn makes, in one place.

#include "frame/frame_boundary.h"

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

ResumableGuestCall &FrameCallState::fieldCall() {
  if (!fieldCall_.has_value()) {
    fieldCall_.emplace(core_);
  }
  return *fieldCall_;
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
  core_.rsub.otAttr.beginLogicFrame(frame);
}

void CoreFrameBoundary::sampleInput() {
  core_.game->pad.serviceFrame();
  // Retail republishes the guest pad packet once per VBlank; this title's decoder reads that
  // buffer, so the publish has to ride the same per-frame service that advances the host pad.
  context(core_).pad.service(core_);
}

void CoreFrameBoundary::tickDisplayField() {
  // Timing::frameTick mirrors another title's linked-libetc address. Toy Story 2's own VSync body
  // proves its counter at 0x8009FD54, so this title owner advances the shared host count and the
  // correct guest compatibility mirror directly. Presentation pacing advances EmulatedTime once,
  // with the complete two-field quota, at the single commit below.
  ++core_.game->timing.vblank;
  ++fieldsDelivered_;
  core_.mem_w32(facts::kVSyncQueryCounter, core_.game->timing.vblank);
  // Learn the guest's live display width and re-centre the horizontal projection for this field,
  // before the guest transforms its vertices: the guest publishes OFX once at graphics init and
  // never moves it again, so at 16:9 the projection would otherwise stay centred on the 512-wide
  // 4:3 canvas. Both are no-ops in the 4:3 leg. The widening is the resident frame's: the front end
  // publishes 512-wide screens of its own, and widening those reflowed a menu instead of a view.
  context(core_).widescreen.syncToGuestDisplay(core_, residentLeg());
  context(core_).widescreen.beginField(core_);
}

void CoreFrameBoundary::serviceDeferredDisplay() {
  if (state_.outerLoop_.phase == OuterLoopPhase::introMovies) {
    return; // the FMV overlay owns the display; 0x80021028 belongs to the resident front end
  }
  if (state_.fieldCall().active()) {
    // A guest call suspended between fields (the front-end poll) owns its own display loop and takes
    // its field callbacks from the executor. A host-initiated guest call here would clobber the
    // caller-saved registers the suspended call resumes with.
    return;
  }
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

void CoreFrameBoundary::updateResidentGame() {
  stepOuterLoop(state_.outerLoop_, *this);
}

void CoreFrameBoundary::advanceAudio() {
  core_.game->spu_audio.frame();
}

void CoreFrameBoundary::present(int guestFields) {
  // A native movie owns the display for as long as it plays. Its frame was delivered from inside
  // the guest call, which is BEFORE this present, so the host's own present would cover it with the
  // guest's 2D layers. Present the guest's layers as usual — they are real and the ledger must see
  // them delivered — then put the movie back on top, which is the layer the player watches.
  const bool movieOnScreen = core_.game->fmv.presenting();
  // The guest re-publishes its drawing environment, drawing offset and display origin every frame,
  // alternating one 512-wide canvas between the two halves of VRAM. The wide canvas is asserted
  // here, after the update and before the captured GP0 stream is rasterized, so the whole frame is
  // rasterized into and presented from the same 684 columns. A no-op in the 4:3 leg.
  context(core_).widescreen.presentField(core_);
  // The temporal presenter must be handed to commit explicitly. The two-argument overload forwards
  // `temporal = nullptr`, which takes the real-frame path every time: the presenter is never
  // invoked, it never commits a field, and its previous-endpoint flag never rises — so a declared
  // fps60 capability silently does nothing.
  core_.game->presentation.commit(&core_, guestFields, core_.game->temporalPresentation.get());
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
  // Retail 0x8007BC74(10,0) loads the finite LEVEL00/LEVEL.RAW asset corpus through
  // 0x8003D88C -> 0x8003B544 -> 0x80021190. The exact 160,484-byte input decodes seven
  // CRC-verified chunks; one guest-turn budget cuts a live back-reference decode mid-chunk.
  // Continue this initialization transaction with its original return sentinel. Ordinary
  // per-frame guest calls still require a return within one turn.
  const std::array loadArguments{10u, 0u};
  callMemoryDispatcher(loadArguments[0], loadArguments[1], "cold front-end asset load");
  state_.introMovieStep_ = kIntroMovieSteps.size();
  if (core_.mem_r32(kFrontEndEvent) != 9) {
    state_.introMovieStep_ = 0;
  }
}

// The intro sequence is MEMORY status 2, 0, 1 and then the queued screen. Each enters the FMV
// overlay, whose loop plays a whole movie inside one guest call and waits on VSync 0x80088628 once
// per movie frame, so one step here delivers one display field. The overlay's own player is
// replaced natively, which yields one movie frame per host turn instead — that is a hostSlice, and
// it takes the same step. A status call returning nonzero ends the sequence, as the guest's own
// `&&` chain does.
bool CoreFrameBoundary::stepIntroMovies() {
  while (true) {
    if (!state_.fieldCall().active()) {
      if (state_.introMovieStep_ >= kIntroMovieSteps.size()) {
        return true;
      }
      const MovieStep &step = kIntroMovieSteps[state_.introMovieStep_];
      const std::array arguments{step.a0, step.a1, step.a2, 0u};
      state_.fieldCall().begin({step.address, 0x8007A9E8u, arguments, std::nullopt, "front-end movie"});
    }
    const auto progress = state_.fieldCall().advance();
    if (progress == ResumableGuestCall::Progress::fieldBoundary ||
        progress == ResumableGuestCall::Progress::hostSlice) {
      return false;
    }
    const bool gated = kIntroMovieSteps[state_.introMovieStep_].address == kMemoryStatus;
    state_.introMovieStep_ =
        gated && state_.fieldCall().result() != 0 ? kIntroMovieSteps.size() : state_.introMovieStep_ + 1;
  }
}

void CoreFrameBoundary::finishColdFrontEnd() {
  core_.mem_w32(0x800A1670u, 1);
  prepareFrontEnd();
}

void CoreFrameBoundary::prepareFrontEnd() {
  // Both calls decode the front end's asset set (0x80021190 DecompressRAW), which takes many guest
  // turns, so they are finite initialization transactions like the cold asset load.
  callFiniteInitialization(kPrepareMemoryState, "front-end memory-state preparation", kMemoryState);
  callFiniteInitialization(kCommitMemoryState, "front-end memory-state commit", kMemoryState);
  core_.mem_w32(kFrontEndEvent, 0);
  core_.mem_w32(kSelectionActive, 0);
  callGuest(core_, kResetGraphics);
}

// The front-end poll (MEMORY 0x800D92C4) is the title screen's own loop: it draws, waits on VSync
// and reads the pad every field until a selection, a timeout or a demo request, and only then
// returns the event. It therefore spans display fields like a movie, and each field is presented
// and sampled by the host between steps; nullopt means the poll is still running.
std::optional<int> CoreFrameBoundary::pollFrontEndEvent() {
  if (!state_.fieldCall().active()) {
    const std::array arguments{2u, 0u};
    state_.fieldCall().begin({kMemoryDispatcher, 0x8007A9E8u, arguments, std::nullopt, "front-end poll"});
    context(core_).yieldAtFieldBarrier = true;
  }
  if (state_.fieldCall().advance() != ResumableGuestCall::Progress::returned) {
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
  core_.mem_w16(kPlaybackLevel, selection);
  phase = (phase + 1) % std::size(kPhaseSelections);
  core_.mem_w32(kPlaybackPhase, phase);
  core_.mem_w32(kLevelId, levelId(selection));
  callGuest(core_, kPlaybackSetup);
  core_.mem_w32(0x800A1640u, static_cast<uint32_t>(-2));
  core_.mem_w32(0x800A163Cu, 0);
  // The same two asset decoders `prepareFrontEnd` runs, on the same finite initialization owner:
  // playback mode reaches this pair through `selectPlaybackLevel`, and a per-frame budget here cut
  // the decode mid-chunk exactly as it did on the post-level route.
  callFiniteInitialization(kPrepareMemoryState, "playback memory-state preparation", kMemoryState);
  callFiniteInitialization(kCommitMemoryState, "playback memory-state commit", kMemoryState);
  core_.mem_w16(kPlaybackLevel, selection);
}

bool CoreFrameBoundary::needsInteractiveSelection() const {
  return !playbackMode() && static_cast<int32_t>(core_.mem_r32(kBootCountdown)) < 0;
}

// The main loop's selection step 0x8007AD8C is two resumable guest calls: the screen loop
// 0x80041240, which waits on the field barrier (fades, transitions, its asset load) and so yields
// to the host at each barrier, and the queued transition screen, whose movie waits on VSync. Each
// spans display fields exactly like the front-end poll and is presented one field at a time.
// A NONZERO return of the screen loop means the player backed out: the retail loop stores -1 as the
// front-end event and re-enters the poll. A zero return means a level was chosen, optionally
// followed by its transition screen, and then the level is prepared.
SelectionProgress CoreFrameBoundary::stepInteractiveSelection() {
  if (!state_.fieldCall().active()) {
    beginSelectionCall();
  }
  if (state_.fieldCall().advance() != ResumableGuestCall::Progress::returned) {
    return SelectionProgress::pending;
  }
  context(core_).yieldAtFieldBarrier = false;
  if (state_.selectionCall_ == SelectionCall::queuedScreen) {
    state_.selectionCall_ = SelectionCall::screenLoop;
    return SelectionProgress::chosen;
  }
  if (state_.fieldCall().result() != 0) {
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
  // Retail 0x8007AE08 stores and passes the same selected level to 0x8007BEC4. The finite owner
  // receives that live argument directly; dropping it forces absent LEVEL00/LEVEL.DAT retries.
  const ResidentPreparationProgress progress =
      state_.residentPreparation_.step(core_, level, static_cast<int>(core_.mem_r32(kPlaybackMode)));
  if (progress != ResidentPreparationProgress::ready) {
    return progress;
  }

  // The retail caller's entry state for the play loop, DECOMPILED WHOLE (Ghidra, exact bytes, in
  // 0x8007A9E8 immediately after its `jal 0x8007bec4` at 0x8007AE14): it clears the transition flags
  // and the per-level counters, publishes `[0x800A1174] = 0`, arms the exit countdown at
  // `[0x800A155C] = 90` and `[0x800A1430] = [0x800C166C] << 1`, and begins the fade. `ResidentPreparation`
  // now RUNS that block instead of this step replaying it, so those words are written by the guest
  // stores that own them -- including the halfword granularity and the `[0x800A1430]` value the
  // hand-written word stores did not reproduce. The loop those words feed -- `FUN_8003FA68(2)` then
  // `FUN_8007B254`/`FUN_8007B850` -- is the resident main loop, which the frame driver owns as
  // `updateResident` plus the deferred field service; the guest is left parked in its own barrier.
  context(core_).camera.reset();
  context(core_).scene.reset();
  return ResidentPreparationProgress::ready;
}

void CoreFrameBoundary::showMemoryDialog() {
  callMemoryDispatcher(9, 0x80, "memory dialog asset load");
}

// THE MODAL OVERLAY SCREENS ARE ONE GUEST CALL EACH, AND BOTH SPAN DISPLAY FIELDS. Ghidra, exact
// bytes: 0x800415E4 (MEMORY CARD, 36 instructions) and 0x8004171C (LOAD/SAVE, 63 instructions) are
// each an asset load through 0x8003D88C followed by the screen's OWN display loop --
// func_0x800def6c() and a while(true) that both consume exactly one 0x8003FA68 field barrier per
// iteration and return only when the player backs out. The decode alone (0x8003B544 -> 0x80021190)
// also cannot finish inside one host turn. So both are driven as resumable field calls exactly like
// the screen loop at 0x80041240, with the installed field-barrier override yielding to the host: ONE
// STEP, ONE DISPLAY FIELD. Every original call in the guest's body still runs through the seam;
// nothing here reimplements the load, the decode, or a screen's UI.
void CoreFrameBoundary::beginMemorySelection() {
  beginOverlayCall(kCheckSave, "memory-card selection");
  state_.selectionCall_ = SelectionCall::memoryCardOverlay;
}

void CoreFrameBoundary::beginLoadSaveSelection() {
  beginOverlayCall(kLoadSave, "load-save selection");
  state_.selectionCall_ = SelectionCall::loadSaveOverlay;
}

bool CoreFrameBoundary::stepMemoryScreen() {
  const ResumableGuestCall::Progress progress = state_.fieldCall().advance();
  if (progress != ResumableGuestCall::Progress::returned) {
    return false;
  }
  context(core_).yieldAtFieldBarrier = false;
  // The MEMORY CARD screen publishes the front end's selection-active word itself, exactly as the
  // one-turn call did, so the level that follows is keyed the same way it always was.
  if (state_.selectionCall_ == SelectionCall::memoryCardOverlay && state_.fieldCall().result() != 0) {
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
  context(core_).scene.beginFrame();
  context(core_).projectionScopes.beginFrame();
  const std::array noArguments{0u, 0u, 0u, 0u};
  callFiniteGuestToReturn(core_,
                          {residentUpdateAddress(alternate), 0x8007A9E8u, noArguments, std::nullopt, "resident update"},
                          kResidentUpdateSliceLimit);
  // Both resident update owners call camera producer 0x8002C848 before the later scene root
  // 0x8002A070. Capture its authored input after the update so future native producers and temporal
  // presentation share one previous/current source rather than re-reading mutable guest RAM.
  context(core_).camera.capture(core_);
  context(core_).scene.finishFrame();
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
    state_.fieldCall().begin({kQueueScreen, 0x8007A9E8u, arguments, std::nullopt, "queued selection screen"});
    return;
  }
  core_.mem_w32(kFrontEndEvent, 0);
  core_.mem_w32(kSelectionActive, 1);
  state_.fieldCall().begin({kInteractiveSelection, 0x8007A9E8u, {}, std::nullopt, "interactive selection"});
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

// Retail exit reason 1 is the sequence level's own end-of-level route. `level % 3 == 0` is the
// inter-level screen route, which is two one-turn calls plus a queued screen and needs no
// transition; every other level loads the next asset set and draws the transition screen, which is
// a field-spanning guest call owned by `pollLevelTransitionEvent`.
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

// Retail 0x8007BC74(4, 0x40), the other end of that route: it loads the next asset set and then
// draws the transition screen, waiting on its own field barrier between screens. That is a
// field-spanning guest call, exactly like the front-end poll, so it is begun here and advanced one
// display field per step below, which then runs the two one-turn bookkeeping calls that follow it in
// `beginSequenceLevel` and reports whether the boot countdown still has fields to run.
std::optional<int> CoreFrameBoundary::pollLevelTransitionEvent() {
  if (!state_.fieldCall().active()) {
    const std::array arguments{4u, 0x40u};
    state_.fieldCall().begin({kMemoryDispatcher, 0x8007A9E8u, arguments, std::nullopt, "level transition"});
    context(core_).yieldAtFieldBarrier = true;
  }
  if (state_.fieldCall().advance() != ResumableGuestCall::Progress::returned) {
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

// The leg the host is presenting: the 3D resident frame and the level start that hands over to it.
// Everything before that — movies, the title, the front-end poll, the Level map — is the guest's
// own 2D/4:3 front end, and the 16:9 widening is not its owner.
bool CoreFrameBoundary::residentLeg() const {
  return state_.outerLoop_.phase == OuterLoopPhase::residentSetup ||
         state_.outerLoop_.phase == OuterLoopPhase::resident;
}

void CoreFrameBoundary::callFiniteInitialization(uint32_t address, std::string_view owner, uint32_t a0) {
  const std::array arguments{a0};
  callFiniteGuestToReturn(
      core_, {address, 0x8007A9E8u, arguments, std::nullopt, owner}, kFiniteInitializationSliceLimit);
}

// Retail 0x8007BC74 is the memory dispatcher. Argument 10 (the cold front end) only loads an asset
// set through 0x8003D88C -> 0x8003B544 -> 0x80021190, whose live back-reference decode no one-turn
// budget can finish: the cold LEVEL00/LEVEL.RAW corpus is 160,484 bytes and seven CRC-verified
// chunks, 15.2M cycles over 28 host turns. So a call that only loads is a finite initialization
// transaction on the shared bound. The arguments that go on to DRAW a screen (4, the post-level
// transition; 2, the title poll; 0/1, the resident movies) are field-spanning instead, and are
// driven as resumable field calls so their own field barrier delivers a present.
void CoreFrameBoundary::callMemoryDispatcher(uint32_t a0, uint32_t a1, std::string_view owner) {
  const std::array arguments{a0, a1, 0u, 0u};
  callFiniteGuestToReturn(
      core_, {kMemoryDispatcher, 0x8007A9E8u, arguments, std::nullopt, owner}, kFiniteInitializationSliceLimit);
}

void CoreFrameBoundary::beginOverlayCall(uint32_t address, const char *owner) {
  if (state_.fieldCall().active()) {
    return;
  }
  state_.fieldCall().begin({address, 0x8007A9E8u, {}, std::nullopt, owner});
  context(core_).yieldAtFieldBarrier = true;
}

} // namespace ts2
