#include "loop/toystory2_frame_driver.h"

#include "boot/guest_main_boot.h"
#include "core.h"
#include "game.h"
#include "game_runtime.h"
#include "guest_execution.h"
#include "guest_facts.h"
#include "loop/outer_loop.h"
#include "loop/resident_frame.h"
#include "loop/resident_preparation.h"
#include "toystory2_context.h"

#include <array>
#include <cstdlib>
#include <iterator>
#include <lucent/log.h>
#include <optional>

namespace ts2 {
namespace {

// Exact resident-main facts from 0x8007AEAC..0x8007AEFC and its two-field barrier 0x8003FA68.
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

uint32_t callGuest(Core &core, uint32_t address, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
  const std::array arguments{a0, a1, a2, a3};
  return callGuestToReturn(core, {address, 0x8007A9E8u, arguments, std::nullopt, "frame driver"});
}

// Which resumable guest call one interactive-selection iteration is currently running.
enum class SelectionCall { screenLoop, queuedScreen };

class CoreResidentFrameBoundary final : public ResidentFrameBoundary, public OuterLoopBoundary {
public:
  CoreResidentFrameBoundary(Core &core,
                            OuterLoopState &outerLoop,
                            ResidentPreparation &residentPreparation,
                            ResumableGuestCall &fieldCall,
                            std::size_t &introMovieStep,
                            SelectionCall &selectionCall)
      : core_(core), outerLoop_(outerLoop), residentPreparation_(residentPreparation), fieldCall_(fieldCall),
        introMovieStep_(introMovieStep), selectionCall_(selectionCall) {}

  int displayFieldQuota() const override {
    // The resident update and the selection screen both wait two fields per iteration.
    const bool pacedAtTwoFields =
        outerLoop_.phase == OuterLoopPhase::resident || outerLoop_.phase == OuterLoopPhase::interactiveSelection;
    return pacedAtTwoFields ? 2 : 1;
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
    core_.mem_w32(facts::kVSyncQueryCounter, core_.game->timing.vblank);
    // Learn the guest's live display width and re-centre the horizontal projection for this field,
    // before the guest transforms its vertices: the guest publishes OFX once at graphics init and
    // never moves it again, so at 16:9 the projection would otherwise stay centred on the 512-wide
    // 4:3 canvas. Both are no-ops in the 4:3 leg.
    context(core_).widescreen.syncToGuestDisplay(core_);
    context(core_).widescreen.beginField(core_);
  }

  void serviceDeferredDisplay() override {
    if (outerLoop_.phase == OuterLoopPhase::introMovies) {
      return; // the FMV overlay owns the display; 0x80021028 belongs to the resident front end
    }
    if (fieldCall_.active()) {
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

  void updateResidentGame() override {
    stepOuterLoop(outerLoop_, *this);
  }

  void advanceAudio() override {
    core_.game->spu_audio.frame();
  }

  void present(int guestFields) override {
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
  }

  void initializeFrontEnd() override {
    finishGuestMainBoot(core_);
    restartColdFrontEnd();
  }

  void restartColdFrontEnd() override {
    core_.mem_w32(kPlaybackMode, 0);
    core_.mem_w32(kFrontEndEvent, 0);
    // Retail 0x8007BC74(10,0) loads the finite LEVEL00/LEVEL.RAW asset corpus through
    // 0x8003D88C -> 0x8003B544 -> 0x80021190. The exact 160,484-byte input decodes seven
    // CRC-verified chunks; one guest-turn budget cuts a live back-reference decode mid-chunk.
    // Continue this initialization transaction with its original return sentinel. Ordinary
    // per-frame guest calls still require a return within one turn.
    const std::array loadArguments{10u, 0u};
    callFiniteGuestToReturn(core_,
                            {kMemoryDispatcher, 0x8007A9E8u, loadArguments, std::nullopt, "cold front-end asset load"},
                            kFiniteInitializationSliceLimit);
    introMovieStep_ = kIntroMovieSteps.size();
    if (core_.mem_r32(kFrontEndEvent) != 9) {
      introMovieStep_ = 0;
    }
  }

  // The intro sequence is MEMORY status 2, 0, 1 and then the queued screen. Each enters the FMV
  // overlay, whose loop plays a whole movie inside one guest call and waits on VSync 0x80088628 once
  // per movie frame, so one step here delivers one display field. A status call returning nonzero
  // ends the sequence, as the guest's own `&&` chain does.
  bool stepIntroMovies() override {
    while (true) {
      if (!fieldCall_.active()) {
        if (introMovieStep_ >= kIntroMovieSteps.size()) {
          return true;
        }
        const MovieStep &step = kIntroMovieSteps[introMovieStep_];
        const std::array arguments{step.a0, step.a1, step.a2, 0u};
        fieldCall_.begin({step.address, 0x8007A9E8u, arguments, std::nullopt, "front-end movie"});
      }
      if (fieldCall_.advance() == ResumableGuestCall::Progress::fieldBoundary) {
        return false;
      }
      const bool gated = kIntroMovieSteps[introMovieStep_].address == kMemoryStatus;
      introMovieStep_ = gated && fieldCall_.result() != 0 ? kIntroMovieSteps.size() : introMovieStep_ + 1;
    }
  }

  void finishColdFrontEnd() override {
    core_.mem_w32(0x800A1670u, 1);
    prepareFrontEnd();
  }

  void callFiniteInitialization(uint32_t address, std::string_view owner, uint32_t a0) {
    const std::array arguments{a0};
    callFiniteGuestToReturn(
        core_, {address, 0x8007A9E8u, arguments, std::nullopt, owner}, kFiniteInitializationSliceLimit);
  }

  void prepareFrontEnd() override {
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
  std::optional<int> pollFrontEndEvent() override {
    if (!fieldCall_.active()) {
      const std::array arguments{2u, 0u};
      fieldCall_.begin({kMemoryDispatcher, 0x8007A9E8u, arguments, std::nullopt, "front-end poll"});
      context(core_).yieldAtFieldBarrier = true;
    }
    if (fieldCall_.advance() == ResumableGuestCall::Progress::fieldBoundary) {
      return std::nullopt;
    }
    context(core_).yieldAtFieldBarrier = false;
    const auto event = static_cast<int>(core_.mem_r32(kFrontEndEvent));
    lucent::info("ts2-frame", "front-end poll returned event {}", event);
    return event;
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

  // The main loop's selection step 0x8007AD8C is two resumable guest calls: the screen loop
  // 0x80041240, which waits on the field barrier (fades, transitions, its asset load) and so yields
  // to the host at each barrier, and the queued transition screen, whose movie waits on VSync. Each
  // spans display fields exactly like the front-end poll and is presented one field at a time.
  // A NONZERO return of the screen loop means the player backed out: the retail loop stores -1 as the
  // front-end event and re-enters the poll. A zero return means a level was chosen, optionally
  // followed by its transition screen, and then the level is prepared.
  SelectionProgress stepInteractiveSelection() override {
    if (!fieldCall_.active()) {
      beginSelectionCall();
    }
    if (fieldCall_.advance() == ResumableGuestCall::Progress::fieldBoundary) {
      return SelectionProgress::pending;
    }
    context(core_).yieldAtFieldBarrier = false;
    if (selectionCall_ == SelectionCall::queuedScreen) {
      selectionCall_ = SelectionCall::screenLoop;
      return SelectionProgress::chosen;
    }
    if (fieldCall_.result() != 0) {
      core_.mem_w32(kFrontEndEvent, static_cast<uint32_t>(-1));
      return SelectionProgress::backToFrontEnd;
    }
    if (!queuedScreenFor(core_.mem_r16s(kPlaybackLevel))) {
      return SelectionProgress::chosen;
    }
    selectionCall_ = SelectionCall::queuedScreen;
    return SelectionProgress::pending;
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
    context(core_).projectionScopes.beginFrame();
    const std::array noArguments{0u, 0u, 0u, 0u};
    callFiniteGuestToReturn(
        core_,
        {residentUpdateAddress(alternate), 0x8007A9E8u, noArguments, std::nullopt, "resident update"},
        kResidentUpdateSliceLimit);
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
  // The transition screen retail queues after a selection that is neither a sequence level nor
  // already queued; the last level is the finale screen.
  static std::optional<uint32_t> queuedScreenFor(int selection) {
    const int screen = selection == 11 ? 16 : selection + 1;
    if ((screen % 3) == 0 && selection != 11) {
      return std::nullopt;
    }
    return static_cast<uint32_t>(screen);
  }

  void beginSelectionCall() {
    if (selectionCall_ == SelectionCall::queuedScreen) {
      const uint32_t screen = *queuedScreenFor(core_.mem_r16s(kPlaybackLevel));
      const std::array arguments{screen, 0u, 0u, 0u};
      fieldCall_.begin({kQueueScreen, 0x8007A9E8u, arguments, std::nullopt, "queued selection screen"});
      return;
    }
    core_.mem_w32(kFrontEndEvent, 0);
    core_.mem_w32(kSelectionActive, 1);
    fieldCall_.begin({kInteractiveSelection, 0x8007A9E8u, {}, std::nullopt, "interactive selection"});
    context(core_).yieldAtFieldBarrier = true;
  }

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
  ResumableGuestCall &fieldCall_;
  std::size_t &introMovieStep_;
  int fieldsDelivered_ = 0;
  SelectionCall &selectionCall_;
};

class ToyStory2FrameDriver final : public FrameDriver {
public:
  void stepFrame(Core &core, uint32_t frame) override {
    if (!fieldCall_) {
      fieldCall_.emplace(core);
    }
    CoreResidentFrameBoundary boundary(
        core, outerLoop_, residentPreparation_, *fieldCall_, introMovieStep_, selectionCall_);
    stepResidentFrame(boundary, frame);
  }

private:
  OuterLoopState outerLoop_;
  ResidentPreparation residentPreparation_;
  std::optional<ResumableGuestCall> fieldCall_;
  std::size_t introMovieStep_ = 0;
  SelectionCall selectionCall_ = SelectionCall::screenLoop;
};

} // namespace

std::unique_ptr<FrameDriver> createFrameDriver(Game &) {
  return std::make_unique<ToyStory2FrameDriver>();
}

} // namespace ts2
