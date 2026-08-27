// Hermetic boundary for Toy Story 2's native-owned resident loop. It exercises the production
// sequencing function with a recorder and constructs the real runtime product to prove the shared
// createFrameDriver seam is populated. No generated code, guest RAM, audio, GPU, disc, or window.

#include "game.h"
#include "game_runtime.h"
#include "input/native_pad_owner.h"
#include "loop/outer_loop.h"
#include "loop/resident_frame.h"
#include "render/guest_widescreen.h"
#include "render/resident_camera_history.h"
#include "render/resident_scene_history.h"
#include "render_capabilities.h"
#include "testutil.h"
#include "toystory2_context.h"
#include "toystory2_runtime.h"

#include <memory>
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
  void prepareFrontEnd() override {
    operations.emplace_back("prepare-front-end");
  }
  int pollFrontEndEvent() override {
    operations.emplace_back("poll");
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
  bool stepInteractiveSelection() override {
    operations.emplace_back("interactive-step");
    return interactiveReady;
  }
  ts2::ResidentPreparationProgress prepareResident() override {
    operations.emplace_back("prepare-resident");
    return preparationProgress;
  }
  void showMemoryDialog() override {
    operations.emplace_back("memory-dialog");
  }
  void checkSaveSelection() override {
    operations.emplace_back("check-save");
  }
  void loadSaveSelection() override {
    operations.emplace_back("load-save");
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
  void shutdown() override {
    operations.emplace_back("shutdown");
  }

  int event = 0;
  bool playback = true;
  bool needsInteractive = true;
  bool interactiveReady = false;
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
      "begin:37", "input", "field", "field", "deferred-display", "resident-update", "audio", "present:2"};
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
      "begin:9", "input", "field", "deferred-display", "resident-update", "audio", "present:1"};
  CHECK(boundary.operations == expected);
}

static void test_runtime_supplies_title_frame_driver() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();

  CHECK(game->runtime == &runtime);
  CHECK(game->frameDriver != nullptr);
}

static void test_runtime_declares_guest_widescreen_without_false_native_or_lerp_claims() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();

  const RenderCapabilities capabilities = runtime.renderCapabilities();
  CHECK(capabilities.defaultPath == RenderPath::Gte);
  CHECK(!capabilities.nativeRenderPath);
  CHECK(!capabilities.temporalInterpolation);

  const GuestWidescreenProjection *policy = runtime.guestWidescreenProjection();
  CHECK(policy != nullptr);
  game->mods.aspect = ASPECT_16_9;
  CHECK(policy->presentationAspect(game->core) == PresentationAspect::Wide16x9);

  const GuestProjectionPlan wide = guest_projection_plan({
      .path = RenderPath::Gte,
      .requested = policy->presentationAspect(game->core),
      .nativePresentation = {320, 240},
      .nativeProjection = {.extent = {512, 240}, .drawWidth = 512},
      .sink = {960, 720},
      .vramWidth = 1024,
  });
  CHECK_EQ(wide.presentationExtent.width, 428);
  CHECK_EQ(wide.projectionExtent.width, 684);
  CHECK_EQ(wide.guestDrawWidth, 684);
  CHECK_EQ(wide.projectionCenterX, 342);
  CHECK_EQ(wide.presentationHorizontalMargin, 54);
  CHECK_EQ(wide.projectionHorizontalMargin, 86);

  const GuestProjectionPlan reference = guest_projection_plan({
      .path = RenderPath::Psx,
      .requested = policy->presentationAspect(game->core),
      .nativePresentation = {320, 240},
      .nativeProjection = {.extent = {512, 240}, .drawWidth = 512},
      .sink = {960, 720},
      .vramWidth = 1024,
  });
  CHECK(!reference.widescreen());
  CHECK_EQ(reference.presentationExtent.width, 320);
  CHECK_EQ(reference.projectionCenterX, 256);
}

static void test_native_pad_owner_publishes_and_decodes_digital_packet() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  game->pad.setButtons(0xBFFFu);
  ts2::initializeNativePad(core);
  CHECK_EQ(core.mem_r16(0x800A109Cu), 1u);
  CHECK_EQ(core.mem_r8(0x800CF8A0u), 0u);
  CHECK_EQ(core.mem_r8(0x800CF8A1u), 0x41u);
  CHECK_EQ(core.mem_r16(0x800CF8A2u), 0xBFFFu);
  CHECK_EQ(core.mem_r8(0x800CF8C8u), 0xFFu);
  CHECK_EQ(ts2::decodeNativeDigitalPad(core), 0x4000u);

  ts2::shutdownNativePad(core);
  CHECK_EQ(core.mem_r16(0x800A109Cu), 0u);
  CHECK_EQ(ts2::decodeNativeDigitalPad(core), 0u);
}

static void test_resident_camera_history_reads_authored_state_and_interpolates_wrap() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  core.mem_w32(0x800C1540u, static_cast<uint32_t>(-100));
  core.mem_w32(0x800C1544u, 200u);
  core.mem_w32(0x800C1548u, 300u);
  core.mem_w16(0x800C154Cu, 4090u);
  core.mem_w16(0x800C154Eu, 100u);
  core.mem_w16(0x800C1550u, 200u);

  ts2::ResidentCameraHistory &history = ts2::context(core).camera;
  history.capture(core);
  CHECK(history.ready());
  CHECK_EQ(history.previous().position[0], -100);
  CHECK_EQ(history.current().rotation[0], 4090u);

  ts2::ResidentCameraSample next = history.current();
  next.position[0] = 100;
  next.position[1] = 400;
  next.position[2] = 700;
  next.rotation[0] = 6;
  history.capture(next);

  const ts2::InterpolatedResidentCamera halfway = history.interpolate(0.5F);
  CHECK(halfway.position[0] == 0.0F);
  CHECK(halfway.position[1] == 300.0F);
  CHECK(halfway.position[2] == 500.0F);
  CHECK(halfway.rotation[0] == 4096.0F);
}

static void test_resident_scene_history_reads_exact_owner_and_mesh_arguments() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  constexpr uint32_t visibilityList = 0x800B0000u;
  constexpr uint32_t visibility = 0x800B0100u;
  constexpr uint32_t object = 0x800B0200u;
  constexpr uint32_t instance = 0x800B0300u;
  constexpr uint32_t resource = 0x800B0400u;
  constexpr uint32_t mesh = 0x800B0500u;
  core.mem_w32(0x800A11E0u, 1u);
  core.mem_w32(visibilityList, visibility);
  core.mem_w8(visibility + 0x0Eu, 0x49u);
  core.mem_w8(visibility + 0x0Fu, 3u);
  core.mem_w32(visibility + 0x10u, object);
  core.mem_w32(object + 0x14u, instance);
  core.mem_w32(object + 0x1Cu, resource);
  core.mem_w32(instance + 0u, static_cast<uint32_t>(-100));
  core.mem_w32(instance + 4u, 200u);
  core.mem_w32(instance + 8u, 300u);
  core.mem_w8(instance + 0x18u, 0xA5u);
  core.mem_w8(instance + 0x19u, 0xE3u);
  core.mem_w32(instance + 0x1Cu, mesh);

  ts2::ResidentSceneHistory &history = ts2::context(core).scene;
  history.beginFrame();
  history.captureOwnerSubmission(core, visibilityList, 1u, 0x801BBFECu, 0u);
  history.captureMeshSubmission(mesh, -12, 7u, 0x80097DC8u, 0x1F800384u);
  history.finishFrame();

  CHECK(history.ready());
  CHECK_EQ(history.current().batches().size(), 1u);
  CHECK_EQ(history.current().candidates().size(), 1u);
  CHECK_EQ(history.current().meshes().size(), 1u);
  const ts2::ResidentSceneCandidate &candidate = history.current().candidates()[0];
  CHECK_EQ(candidate.visibilityAddress, visibility);
  CHECK_EQ(candidate.objectAddress, object);
  CHECK_EQ(candidate.resourceAddress, resource);
  CHECK_EQ(candidate.instanceAddress, instance);
  CHECK_EQ(candidate.meshAddress, mesh);
  CHECK_EQ(candidate.position[0], -100);
  CHECK_EQ(candidate.position[1], 200);
  CHECK_EQ(candidate.position[2], 300);
  CHECK_EQ(candidate.type, 0x49u);
  CHECK_EQ(candidate.viewport, 3u);
  CHECK_EQ(candidate.material, 0xA5u);
  CHECK_EQ(candidate.scaleFlags, 0xE3u);
  CHECK_EQ(history.current().meshes()[0].meshAddress, mesh);
  CHECK_EQ(history.current().meshes()[0].headerWord, -12);
  CHECK_EQ(history.current().meshes()[0].scale, 7u);
  CHECK_EQ(history.previous().candidates()[0].meshAddress, mesh);
}

static void test_outer_loop_reaches_normal_resident_in_finite_steps() {
  ts2::OuterLoopState state;
  RecordingOuterLoop boundary;

  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::pollFrontEnd);
  boundary.event = 0;
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::residentSetup);
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::resident);
  ts2::stepOuterLoop(state, boundary);

  const std::vector<std::string> expected = {
      "initialize", "poll", "playback:on", "ack-entry", "select-playback", "prepare-resident", "resident-update"};
  CHECK(boundary.operations == expected);
}

static void test_outer_loop_interactive_path_yields_between_selection_iterations() {
  ts2::OuterLoopState state{ts2::OuterLoopPhase::pollFrontEnd};
  RecordingOuterLoop boundary;
  boundary.event = 1;

  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::interactiveSelection);
  ts2::stepOuterLoop(state, boundary);
  CHECK(state.phase == ts2::OuterLoopPhase::interactiveSelection);
  boundary.interactiveReady = true;
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
  static constexpr EventExpectation expectations[] = {
      {2, "memory-dialog"}, {3, "check-save"}, {4, "load-save"}, {8, "restart"}};

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
  RUN(runtime_declares_guest_widescreen_without_false_native_or_lerp_claims);
  RUN(native_pad_owner_publishes_and_decodes_digital_packet);
  RUN(resident_camera_history_reads_authored_state_and_interpolates_wrap);
  RUN(resident_scene_history_reads_exact_owner_and_mesh_arguments);
  RUN(outer_loop_reaches_normal_resident_in_finite_steps);
  RUN(outer_loop_interactive_path_yields_between_selection_iterations);
  RUN(resident_preparation_yields_and_can_finish);
  RUN(outer_loop_front_end_events_are_finite_and_non_fallthrough);
  RUN(resident_mode_selects_both_measured_owners);
  RUN(post_resident_routes_are_finite_state_transitions);
  return pt_summary();
}
