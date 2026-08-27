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
    state.phase = OuterLoopPhase::pollFrontEnd;
    return;

  case OuterLoopPhase::coldRestart:
    boundary.restartColdFrontEnd();
    state.phase = OuterLoopPhase::pollFrontEnd;
    return;

  case OuterLoopPhase::frontEndSetup:
    boundary.prepareFrontEnd();
    state.phase = OuterLoopPhase::pollFrontEnd;
    return;

  case OuterLoopPhase::pollFrontEnd: {
    const int event = boundary.pollFrontEndEvent();
    switch (event) {
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
    if (boundary.stepInteractiveSelection()) {
      beginResidentPreparation(state);
    }
    return;

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
