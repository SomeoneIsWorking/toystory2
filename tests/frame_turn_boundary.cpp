// Frame turn boundary: per-field order, the guest main loop's title operations and the runtime factories, run against
// recorders.

#include "frame/outer_loop.h"
#include "frame/resident_frame.h"
#include "game.h"
#include "game_runtime.h"
#include "render_capabilities.h"
#include "render_mode.h"
#include "runtime/toystory2_runtime.h"
#include "testutil.h"
#include "widescreen/guest_widescreen.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

class RecordingBoundary final : public ts2::ResidentFrameBoundary {
public:
  int displayFieldQuota() const override {
    return fieldQuota;
  }
  void beginLogicFrame(uint32_t frame) override {
    operations.push_back("begin:" + std::to_string(frame));
  }
  void sampleInput() override {
    operations.emplace_back("input");
  }
  void tickDisplayField() override {
    operations.emplace_back("field");
  }
  void serviceDeferredDisplay() override {
    operations.emplace_back("deferred-display");
  }
  void updateResidentGame() override {
    operations.emplace_back("resident-update");
  }
  void advanceAudio() override {
    operations.emplace_back("audio");
  }
  void present(int guestFields) override {
    operations.push_back("present:" + std::to_string(guestFields));
  }

  std::vector<std::string> operations;
  int fieldQuota = 2;
};

class RecordingOuterLoop final : public ts2::OuterLoopBoundary {
public:
  void initializeFrontEnd() override {
    operations.emplace_back("initialize");
  }
  void restartColdFrontEnd() override {
    operations.emplace_back("restart-cold");
  }
  bool stepIntroMovies() override {
    operations.emplace_back("intro-movies");
    return introMoviesFinished;
  }
  void finishColdFrontEnd() override {
    operations.emplace_back("finish-cold");
  }
  void prepareFrontEnd() override {
    operations.emplace_back("prepare-front-end");
  }
  std::optional<int> pollFrontEndEvent() override {
    operations.emplace_back("poll");
    if (pollFieldsRemaining > 0) {
      --pollFieldsRemaining;
      return std::nullopt;
    }
    return event;
  }
  void acknowledgeResidentEntry() override {
    operations.emplace_back("ack-entry");
  }
  void finishFrontEndPoll() override {
    operations.emplace_back("finish-poll");
  }
  bool playbackMode() const override {
    return playback;
  }
  void setPlaybackMode(bool enabled) override {
    playback = enabled;
    operations.push_back(enabled ? "playback:on" : "playback:off");
  }
  void selectPlaybackLevel() override {
    operations.emplace_back("select-playback");
  }
  bool needsInteractiveSelection() const override {
    return needsInteractive;
  }
  ts2::SelectionProgress stepInteractiveSelection() override {
    operations.emplace_back("interactive-step");
    return selectionProgress;
  }
  ts2::ResidentPreparationProgress prepareResident() override {
    operations.emplace_back("prepare-resident");
    return preparationProgress;
  }
  void showMemoryDialog() override {
    operations.emplace_back("memory-dialog");
  }
  bool memoryScreenDone_ = true;

  bool stepMemoryScreen() override {
    operations.emplace_back("memory-screen-step");
    return memoryScreenDone_;
  }
  void beginMemorySelection() override {
    operations.emplace_back("begin-check-save");
  }
  void beginLoadSaveSelection() override {
    operations.emplace_back("begin-load-save");
  }
  void restartFrontEnd() override {
    operations.emplace_back("restart");
  }
  bool residentActive() const override {
    return residentIsActive;
  }
  void updateResident() override {
    operations.emplace_back("resident-update");
  }
  ts2::PostResidentTransition finishResident() override {
    operations.emplace_back("finish-resident");
    return postResidentTransition;
  }
  std::optional<int> pollLevelTransitionEvent() override {
    operations.emplace_back("level-transition");
    if (levelTransitionFieldsRemaining > 0) {
      --levelTransitionFieldsRemaining;
      return std::nullopt;
    }
    return levelTransitionEvent;
  }
  void shutdown() override {
    operations.emplace_back("shutdown");
  }

  int event = 0;
  int pollFieldsRemaining = 0;            // fields the poll spans before it returns its event
  int levelTransitionFieldsRemaining = 0; // fields the post-level transition spans before it returns
  int levelTransitionEvent = 1;           // 0 = the boot countdown finished, 1 = prepare the next level
  bool introMoviesFinished = true;
  bool playback = true;
  bool needsInteractive = true;
  ts2::SelectionProgress selectionProgress = ts2::SelectionProgress::pending;
  ts2::ResidentPreparationProgress preparationProgress = ts2::ResidentPreparationProgress::ready;
  bool residentIsActive = true;
  ts2::PostResidentTransition postResidentTransition = ts2::PostResidentTransition::residentSetup;
  std::vector<std::string> operations;
};

} // namespace
static void test_measured_resident_order_has_one_two_field_present() {
  RecordingBoundary boundary;
  ts2::stepResidentFrame(boundary, 37);

  const std::vector<std::string> expected = {
      "begin:37", "input", "field", "field", "deferred-display", "present:2", "resident-update", "audio"};
  CHECK_EQ(boundary.operations.size(), expected.size());
  for (size_t index = 0; index < expected.size(); ++index) {
    CHECK(boundary.operations[index] == expected[index]);
  }
}

static void test_transition_frame_owns_one_field() {
  RecordingBoundary boundary;
  boundary.fieldQuota = 1;
  ts2::stepResidentFrame(boundary, 9);

  const std::vector<std::string> expected = {
      "begin:9", "input", "field", "deferred-display", "present:1", "resident-update", "audio"};
  CHECK(boundary.operations == expected);
}

static void test_runtime_supplies_title_frame_driver() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();

  CHECK(game->runtime == &runtime);
  CHECK(game->frameDriver != nullptr);
}

static void test_runtime_selects_the_record_path_with_guest_widescreen() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();

  // The picture is the guest's GP0 output replayed from the frame record; producers key the in-between.
  const RenderCapabilities capabilities = runtime.renderCapabilities();
  CHECK(capabilities.defaultPath == RenderPath::Record);
  CHECK(!capabilities.nativeRenderPath);
  CHECK(capabilities.temporalInterpolation);
  render_path_install(&game->core);
  CHECK(game->core.rsub.mode.path() == RenderPath::Record);
  CHECK(game->temporalPresentation == nullptr);

  const GuestWidescreenProjection *policy = runtime.guestWidescreenProjection();
  CHECK(policy != nullptr);
  game->mods.aspect = ASPECT_16_9;
  CHECK(policy->presentationAspect(game->core) == PresentationAspect::Wide16x9);

  // The record canvas holds the margins: the guest keeps its retail 512-wide canvas and centre.
  const GuestProjectionPlan wide = guest_projection_plan({
      .path = RenderPath::Record,
      .requested = policy->presentationAspect(game->core),
      .nativePresentation = {512, 240},
      .nativeProjection = {.extent = {512, 240}, .drawWidth = 512},
      .sink = {960, 720},
      .vramWidth = 1024,
  });
  CHECK(wide.widescreen());
  CHECK_EQ(wide.presentationExtent.width, 684);
  CHECK_EQ(wide.presentationHorizontalMargin, 86);
  CHECK_EQ(wide.guestDrawWidth, 512);
  CHECK_EQ(wide.projectionCenterX, 256);

  game->mods.aspect = ASPECT_4_3;
  const GuestProjectionPlan standard = guest_projection_plan({
      .path = RenderPath::Record,
      .requested = policy->presentationAspect(game->core),
      .nativePresentation = {512, 240},
      .nativeProjection = {.extent = {512, 240}, .drawWidth = 512},
      .sink = {960, 720},
      .vramWidth = 1024,
  });
  CHECK(!standard.widescreen());
  CHECK_EQ(standard.presentationHorizontalMargin, 0);
}

static void test_outer_loop_reaches_normal_resident_in_finite_steps() {
  ts2::OuterLoopState state;
  RecordingOuterLoop boundary;

  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::introMovies);
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::pollFrontEnd);
  boundary.event = 0;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::residentSetup);
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::resident);
  ts2::stepOuterLoop(state, boundary);

  const std::vector<std::string> expected = {"initialize",
                                             "intro-movies",
                                             "finish-cold",
                                             "poll",
                                             "playback:on",
                                             "ack-entry",
                                             "select-playback",
                                             "prepare-resident",
                                             "resident-update"};
  CHECK(boundary.operations == expected);
}

static void test_the_level_transition_spans_fields_and_then_prepares_the_next_level() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::resident};
  RecordingOuterLoop boundary;
  boundary.residentIsActive = false;
  boundary.postResidentTransition = ts2::PostResidentTransition::levelTransition;
  boundary.levelTransitionFieldsRemaining = 2;

  // The end of the level hands the phase over in one step and does no work of its own.
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::levelTransition);
  CHECK(boundary.operations == (std::vector<std::string>{"finish-resident"}));

  // Each display field of the transition is one step, and a running transition prepares nothing:
  // the next level is not prepared until the guest's own call has returned.
  for (int field = 0; field < 2; ++field) {
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == ts2::OuterLoopPhase::levelTransition);
  }
  CHECK(boundary.operations == (std::vector<std::string>{"finish-resident", "level-transition", "level-transition"}));
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::residentSetup);
  CHECK(boundary.operations.back() == "level-transition");
}

static void test_a_finished_boot_countdown_at_the_level_transition_ends_the_game() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::levelTransition};
  RecordingOuterLoop boundary;
  boundary.levelTransitionEvent = 0;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::finished);
  CHECK(boundary.operations == (std::vector<std::string>{"level-transition", "shutdown"}));
}

static void test_intro_movies_yield_one_field_per_step_until_finished() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::introMovies};
  RecordingOuterLoop boundary;
  boundary.introMoviesFinished = false;
  for (int step = 0; step < 3; ++step) {
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == ts2::OuterLoopPhase::introMovies);
  }
  CHECK_EQ(boundary.operations.size(), 3u);
  CHECK(boundary.operations.back() == "intro-movies");
  boundary.introMoviesFinished = true;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::pollFrontEnd);
  CHECK(boundary.operations[3] == "intro-movies");
  CHECK(boundary.operations[4] == "finish-cold");
  CHECK_EQ(boundary.operations.size(), 5u);
}

static void test_backing_out_of_the_selection_screen_reenters_the_front_end_poll() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::interactiveSelection};
  RecordingOuterLoop boundary;
  boundary.selectionProgress = ts2::SelectionProgress::backToFrontEnd;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::pollFrontEnd);
  // Going back prepares nothing: the poll's own re-entry path handles the stored -1 event.
  CHECK(boundary.operations == (std::vector<std::string>{"interactive-step"}));
}

static void test_front_end_poll_spans_fields_without_leaving_its_phase_until_it_returns() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::pollFrontEnd};
  RecordingOuterLoop boundary;
  boundary.event = 1;
  boundary.pollFieldsRemaining = 3;
  for (int field = 0; field < 3; ++field) {
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == ts2::OuterLoopPhase::pollFrontEnd);
  }
  // A running poll acts on nothing: no playback flag, no entry acknowledgement, no mode selection.
  CHECK(boundary.operations == (std::vector<std::string>{"poll", "poll", "poll"}));
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::interactiveSelection);
  CHECK(boundary.operations.size() > 4u);
  CHECK(boundary.operations[3] == "poll");
  CHECK(boundary.operations[4] == "playback:off");
}

static void test_outer_loop_interactive_path_yields_between_selection_iterations() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::pollFrontEnd};
  RecordingOuterLoop boundary;
  boundary.event = 1;

  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::interactiveSelection);
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::interactiveSelection);
  boundary.selectionProgress = ts2::SelectionProgress::chosen;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::residentSetup);
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::resident);

  const std::vector<std::string> expected = {
      "poll", "playback:off", "ack-entry", "interactive-step", "interactive-step", "prepare-resident"};
  CHECK(boundary.operations == expected);
}

static void test_resident_preparation_yields_and_can_finish() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::residentSetup};
  RecordingOuterLoop boundary;
  boundary.preparationProgress = ts2::ResidentPreparationProgress::pending;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::residentSetup);
  CHECK(boundary.operations == std::vector<std::string>({"prepare-resident"}));

  boundary.operations.clear();
  boundary.preparationProgress = ts2::ResidentPreparationProgress::finished;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::finished);
  CHECK(boundary.operations == std::vector<std::string>({"prepare-resident", "shutdown"}));
}

static void test_outer_loop_front_end_events_are_finite_and_non_fallthrough() {
  struct EventExpectation {
    int event;
    const char *operation;
  };
  static constexpr EventExpectation expectations[] = {{2, "memory-dialog"}, {8, "restart"}};

  for (const auto &expectation : expectations) {
    ts2::OuterLoopState state{ts2::OuterLoopPhase::pollFrontEnd};
    RecordingOuterLoop boundary;
    boundary.event = expectation.event;
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == ts2::OuterLoopPhase::pollFrontEnd);
    const size_t expectedSize = expectation.event == 8 ? 2u : 3u;
    CHECK_EQ(boundary.operations.size(), expectedSize);
    CHECK(boundary.operations[1] == expectation.operation);
    if (expectation.event != 8) {
      CHECK(boundary.operations[2] == "finish-poll");
    }
  }

  // Events 3 and 4 are modal overlay screens: one guest call spanning display fields, so the poll stays open
  // until the screen returns.
  static constexpr EventExpectation overlayExpectations[] = {{3, "begin-check-save"}, {4, "begin-load-save"}};

  for (const auto &expectation : overlayExpectations) {
    ts2::OuterLoopState state{ts2::OuterLoopPhase::pollFrontEnd};
    RecordingOuterLoop boundary;
    boundary.event = expectation.event;
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == ts2::OuterLoopPhase::memoryScreen);
    // operations[0] is the poll step itself; the begin is what this event dispatched.
    CHECK_EQ(boundary.operations.size(), 2u);
    CHECK(boundary.operations[1] == expectation.operation);

    // The screen is still running: one display field per step, and the poll stays open.
    boundary.memoryScreenDone_ = false;
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == ts2::OuterLoopPhase::memoryScreen);
    CHECK(boundary.operations[2] == "memory-screen-step");

    // The screen returned: only now is the poll finished and the loop back on the front end.
    boundary.memoryScreenDone_ = true;
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == ts2::OuterLoopPhase::pollFrontEnd);
    CHECK(boundary.operations[3] == "memory-screen-step");
    CHECK(boundary.operations[4] == "finish-poll");
  }

  ts2::OuterLoopState finished{ts2::OuterLoopPhase::pollFrontEnd};
  RecordingOuterLoop boundary;
  boundary.event = 9;
  ts2::stepOuterLoop(finished, boundary);
  CHECK(finished.phase == ts2::OuterLoopPhase::finished);
  CHECK(boundary.operations == std::vector<std::string>({"poll", "shutdown"}));
}

static void test_resident_mode_selects_both_measured_owners() {
  CHECK_EQ(ts2::residentUpdateAddress(false), 0x8007B254u);
  CHECK_EQ(ts2::residentUpdateAddress(true), 0x8007B850u);
}

static void test_post_resident_routes_are_finite_state_transitions() {
  struct ExpectedTransition {
    ts2::PostResidentTransition result;
    ts2::OuterLoopPhase phase;
  };
  static constexpr ExpectedTransition transitions[] = {
      {ts2::PostResidentTransition::coldRestart, ts2::OuterLoopPhase::coldRestart},
      {ts2::PostResidentTransition::frontEndSetup, ts2::OuterLoopPhase::frontEndSetup},
      {ts2::PostResidentTransition::residentSetup, ts2::OuterLoopPhase::residentSetup},
      {ts2::PostResidentTransition::levelTransition, ts2::OuterLoopPhase::levelTransition},
      {ts2::PostResidentTransition::finished, ts2::OuterLoopPhase::finished},
  };

  for (const auto &transition : transitions) {
    ts2::OuterLoopState state{ts2::OuterLoopPhase::resident};
    RecordingOuterLoop boundary;
    boundary.residentIsActive = false;
    boundary.postResidentTransition = transition.result;
    ts2::stepOuterLoop(state, boundary);
    CHECK(state.phase == transition.phase);
    CHECK(!boundary.operations.empty());
    CHECK(boundary.operations[0] == "finish-resident");
    const size_t expectedOperations = transition.result == ts2::PostResidentTransition::finished ? 2u : 1u;
    CHECK_EQ(boundary.operations.size(), expectedOperations);
    if (transition.result == ts2::PostResidentTransition::finished) {
      CHECK(boundary.operations[1] == "shutdown");
    }
  }
}

int main() {
  RUN(measured_resident_order_has_one_two_field_present);
  RUN(transition_frame_owns_one_field);
  RUN(runtime_supplies_title_frame_driver);
  RUN(runtime_selects_the_record_path_with_guest_widescreen);
  RUN(outer_loop_reaches_normal_resident_in_finite_steps);
  RUN(intro_movies_yield_one_field_per_step_until_finished);
  RUN(the_level_transition_spans_fields_and_then_prepares_the_next_level);
  RUN(a_finished_boot_countdown_at_the_level_transition_ends_the_game);
  RUN(backing_out_of_the_selection_screen_reenters_the_front_end_poll);
  RUN(front_end_poll_spans_fields_without_leaving_its_phase_until_it_returns);
  RUN(outer_loop_interactive_path_yields_between_selection_iterations);
  RUN(resident_preparation_yields_and_can_finish);
  RUN(outer_loop_front_end_events_are_finite_and_non_fallthrough);
  RUN(resident_mode_selects_both_measured_owners);
  RUN(post_resident_routes_are_finite_state_transitions);
  return pt_summary();
}
