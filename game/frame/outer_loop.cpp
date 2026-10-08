#include "frame/outer_loop.h"

namespace ts2 {
namespace {

void beginResidentPreparation(OuterLoopState &state) {
  state.phase = OuterLoopPhase::residentSetup;
}

void beginCurrentMode(OuterLoopState &state, OuterLoopBoundary &boundary) {
  if (boundary.playbackMode()) {
    boundary.selectPlaybackLevel();
    beginResidentPreparation(state);
    return;
  }
  if (boundary.needsInteractiveSelection()) {
    state.phase = OuterLoopPhase::interactiveSelection;
    return;
  }
  beginResidentPreparation(state);
}

} // namespace

void stepOuterLoop(OuterLoopState &state, OuterLoopBoundary &boundary) {
  switch (state.phase) {
  case OuterLoopPhase::coldSetup:
    boundary.initializeFrontEnd();
    state.phase = OuterLoopPhase::introMovies;
    return;

  case OuterLoopPhase::coldRestart:
    boundary.restartColdFrontEnd();
    state.phase = OuterLoopPhase::introMovies;
    return;

  case OuterLoopPhase::introMovies:
    if (boundary.stepIntroMovies()) {
      boundary.finishColdFrontEnd();
      state.phase = OuterLoopPhase::pollFrontEnd;
    }
    return;

  case OuterLoopPhase::frontEndSetup:
    boundary.prepareFrontEnd();
    state.phase = OuterLoopPhase::pollFrontEnd;
    return;

  case OuterLoopPhase::pollFrontEnd: {
    const std::optional<int> polled = boundary.pollFrontEndEvent();
    if (!polled) {
      return;
    }
    switch (*polled) {
    case 0:
      boundary.setPlaybackMode(true);
      boundary.acknowledgeResidentEntry();
      beginCurrentMode(state, boundary);
      return;
    case 1:
      boundary.setPlaybackMode(false);
      boundary.acknowledgeResidentEntry();
      beginCurrentMode(state, boundary);
      return;
    case 2:
      boundary.showMemoryDialog();
      boundary.finishFrontEndPoll();
      return;
    case 3:
      boundary.beginMemorySelection();
      state.phase = OuterLoopPhase::memoryScreen;
      return;
    case 4:
      state.phase = OuterLoopPhase::memoryScreen;
      boundary.beginLoadSaveSelection();
      return;
    case 8:
      boundary.restartFrontEnd();
      return;
    case 9:
      boundary.shutdown();
      state.phase = OuterLoopPhase::finished;
      return;
    default:
      // Events 5/6/7 and anything else share the guest switch's default leg and keep the playback flag.
      boundary.acknowledgeResidentEntry();
      beginCurrentMode(state, boundary);
      return;
    }
  }

  // Own phase: both overlays share the driver's single resumable call, so re-entering pollFrontEnd
  // would read the overlay's return value as the poll event.
  case OuterLoopPhase::memoryScreen: {
    if (!boundary.stepMemoryScreen()) {
      return;
    }
    boundary.finishFrontEndPoll();
    state.phase = OuterLoopPhase::pollFrontEnd;
    return;
  }

  case OuterLoopPhase::interactiveSelection:
    switch (boundary.stepInteractiveSelection()) {
    case SelectionProgress::pending:
      return;
    case SelectionProgress::chosen:
      beginResidentPreparation(state);
      return;
    case SelectionProgress::backToFrontEnd:
      state.phase = OuterLoopPhase::pollFrontEnd;
      return;
    }
    return;

  case OuterLoopPhase::levelTransition: {
    // Field-spanning call, not a finite one: a finite call supplies barriers without delivering a
    // field, so the guest's screen loop would run free.
    const std::optional<int> event = boundary.pollLevelTransitionEvent();
    if (!event) {
      return;
    }
    if (*event == 0) {
      state.phase = OuterLoopPhase::finished;
      boundary.shutdown();
      return;
    }
    state.phase = OuterLoopPhase::residentSetup;
    return;
  }

  case OuterLoopPhase::residentSetup:
    switch (boundary.prepareResident()) {
    case ResidentPreparationProgress::pending:
      return;
    case ResidentPreparationProgress::ready:
      state.phase = OuterLoopPhase::resident;
      return;
    case ResidentPreparationProgress::finished:
      state.phase = OuterLoopPhase::finished;
      boundary.shutdown();
      return;
    }
    return;

  case OuterLoopPhase::resident:
    if (boundary.residentActive()) {
      boundary.updateResident();
      return;
    }
    switch (boundary.finishResident()) {
    case PostResidentTransition::coldRestart:
      state.phase = OuterLoopPhase::coldRestart;
      return;
    case PostResidentTransition::frontEndSetup:
      state.phase = OuterLoopPhase::frontEndSetup;
      return;
    case PostResidentTransition::levelTransition:
      state.phase = OuterLoopPhase::levelTransition;
      return;
    case PostResidentTransition::residentSetup:
      state.phase = OuterLoopPhase::residentSetup;
      return;
    case PostResidentTransition::finished:
      state.phase = OuterLoopPhase::finished;
      boundary.shutdown();
      return;
    }
    return;

  case OuterLoopPhase::finished:
    return;
  }
}

} // namespace ts2
