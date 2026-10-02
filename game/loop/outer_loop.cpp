#include "loop/outer_loop.h"

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
      boundary.checkSaveSelection();
      boundary.finishFrontEndPoll();
      return;
    case 4:
      boundary.loadSaveSelection();
      boundary.finishFrontEndPoll();
      return;
    case 8:
      boundary.restartFrontEnd();
      return;
    case 9:
      boundary.shutdown();
      state.phase = OuterLoopPhase::finished;
      return;
    default:
      // The measured switch shares its default leg with events 0/1 after retaining the existing
      // playback flag. This includes the deliberately unlabelled 5/6/7 event values.
      boundary.acknowledgeResidentEntry();
      beginCurrentMode(state, boundary);
      return;
    }
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
    // The guest's own post-level transition loads the next asset set and then draws the transition
    // screen, waiting on its field barrier between screens. It is therefore a field-spanning guest
    // call like the front-end poll, presented one field per step, and NOT a finite transaction: a
    // finite call supplies its barriers without delivering a field, so the guest's screen loop ran
    // free (~4,500 display lists in 0.47 s) and the frame's capture overflowed the render queue.
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
