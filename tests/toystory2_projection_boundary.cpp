// Toy Story 2 projection-publication boundary: the title's measured libgte addresses must install
// the shared faithful leaves, mutate CPU/GTE state exactly like retail, and record authored camera
// projection on the same Core. Hermetic: no disc, generated substrate, GPU, window, or game loop.

#include "core.h"
#include "game.h"
#include "hw_bind.h"
#include "platform_hle.h"
#include "render/resident_camera_history.h"
#include "render/resident_projection_scopes.h"
#include "testutil.h"
#include "toystory2_runtime.h"

#include <memory>

namespace {

constexpr uint32_t kSetGeomOffset = 0x80083CD4u;
constexpr uint32_t kSetGeomScreen = 0x80083CF4u;
constexpr uint32_t kProjectionLeavesEnd = 0x80083D00u;
constexpr uint32_t kGpuTimeoutArm = 0x80088380u;
constexpr uint32_t kGpuTimeoutCheck = 0x800883B4u;
constexpr uint32_t kGpuTimeoutDeadline = 0x8009EC20u;
constexpr uint32_t kGpuTimeoutFlag = 0x8009EC24u;
constexpr uint32_t kVSync = 0x80088628u;
constexpr uint32_t kVSyncBodyEnd = 0x80088770u;

std::unique_ptr<Game> freshGame() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  gte_bind(&game->core);
  game->platform_hle.initBuiltins();
  return game;
}

} // namespace

static void test_measured_windows_install_projection_gpu_timeout_and_vsync_owners() {
  auto game = freshGame();

  CHECK(game->platform_hle.lookup(kSetGeomOffset) != nullptr);
  CHECK(game->platform_hle.lookup(kSetGeomScreen) != nullptr);
  CHECK(game->platform_hle.lookup(kGpuTimeoutArm) != nullptr);
  CHECK(game->platform_hle.lookup(kGpuTimeoutCheck) != nullptr);
  CHECK(game->platform_hle.lookup(kVSync) != nullptr);
  CHECK(game->platform_hle.lookup(kSetGeomOffset - 4) == nullptr);
  CHECK(game->platform_hle.lookup(kProjectionLeavesEnd) == nullptr);
  CHECK(game->platform_hle.lookup(kVSync - 4) == nullptr);
  CHECK(game->platform_hle.lookup(kVSyncBodyEnd) == nullptr);
  CHECK(game->platform_hle.lookup(0x8003A650u) == nullptr); // title graphics init is never HLE'd
}

static void test_gpu_timeout_pair_completes_without_guest_vsync() {
  auto game = freshGame();
  Core &core = game->core;
  OverrideFn arm = game->platform_hle.lookup(kGpuTimeoutArm);
  OverrideFn check = game->platform_hle.lookup(kGpuTimeoutCheck);
  CHECK(arm != nullptr);
  CHECK(check != nullptr);

  core.mem_w32(kGpuTimeoutDeadline, 0);
  core.mem_w32(kGpuTimeoutFlag, 9);
  arm(&core);
  CHECK_EQ(core.mem_r32(kGpuTimeoutDeadline), 0x7FFFFFFFu);
  CHECK_EQ(core.mem_r32(kGpuTimeoutFlag), 0u);
  core.r[2] = 0xFFFFFFFFu;
  check(&core);
  CHECK_EQ(core.r[2], 0u);
}

static void test_offset_leaf_preserves_retail_state_and_records_projection() {
  auto game = freshGame();
  Core &core = game->core;
  OverrideFn setOffset = game->platform_hle.lookup(kSetGeomOffset);
  CHECK(setOffset != nullptr);

  core.r[4] = 256u;
  core.r[5] = 120u;
  setOffset(&core);

  CHECK_EQ(core.r[4], 256u << 16);
  CHECK_EQ(core.r[5], 120u << 16);
  CHECK_EQ(gte_read_ctrl(24), 256u << 16);
  CHECK_EQ(gte_read_ctrl(25), 120u << 16);
  CHECK_EQ(core.rsub.projParams.geomOfx(), 256.0f);
  CHECK_EQ(core.rsub.projParams.geomOfy(), 120.0f);
  CHECK(!core.rsub.projParams.geomValid());
}

static void test_screen_leaf_completes_same_core_projection() {
  auto game = freshGame();
  Core &core = game->core;
  OverrideFn setOffset = game->platform_hle.lookup(kSetGeomOffset);
  OverrideFn setScreen = game->platform_hle.lookup(kSetGeomScreen);
  CHECK(setOffset != nullptr);
  CHECK(setScreen != nullptr);

  core.r[4] = 256u;
  core.r[5] = 120u;
  setOffset(&core);
  core.r[4] = 160u;
  setScreen(&core);

  CHECK_EQ(core.r[4], 160u); // retail SetGeomScreen does not mutate a0
  CHECK_EQ(gte_read_ctrl(26), 160u);
  CHECK_EQ(core.rsub.projParams.geomH(), 160.0f);
  CHECK(core.rsub.projParams.geomValid());
}

// The provenance scope of the visibility pass is its occurrence in the field: the guest calls it
// with no arguments, so nothing in the registers distinguishes one call from another, and the count
// restarts with every resident update. Two calls in one field must not share a key (their primitives
// would pair against each other), and the same call in the next field must get the same key again.
static void test_visibility_pass_scope_is_its_occurrence_in_the_field() {
  ts2::render::ResidentProjectionScopes scopes;
  const std::uint64_t first = scopes.passInstance(ts2::render::ResidentProjectionScopes::kVisibilityPass);
  const std::uint64_t second = scopes.passInstance(ts2::render::ResidentProjectionScopes::kVisibilityPass);
  CHECK(first != 0u);
  CHECK(first != second);
  CHECK(scopes.passInstance(ts2::render::ResidentProjectionScopes::kVisibilityPass) != first);
  scopes.beginFrame();
  CHECK_EQ(scopes.passInstance(ts2::render::ResidentProjectionScopes::kVisibilityPass), first);
}

// A modelled producer keys on what the guest uses to tell its instances apart: the first argument,
// plus the call's occurrence in the field, so the same model drawn twice is two instances and the
// count restarts with every resident update.
static void test_modelled_producer_scope_keys_argument_and_occurrence() {
  ts2::render::ResidentProjectionScopes scopes;
  constexpr std::uint32_t kModel = 0x8002AC40u;
  const std::uint64_t modelA = scopes.producerInstance(kModel, 0x80100000u);
  const std::uint64_t modelB = scopes.producerInstance(kModel, 0x80200000u);
  const std::uint64_t modelAAgain = scopes.producerInstance(kModel, 0x80100000u);
  CHECK(modelA != modelB);
  CHECK(modelA != modelAAgain);
  scopes.beginFrame();
  CHECK_EQ(scopes.producerInstance(kModel, 0x80100000u), modelA);
}

// 60 fps continuity is a statement about the GUEST's camera, not about this port's taste. A field
// that continues the last one keeps the camera continuous; a field in which an authored angle moves
// by half the guest's own turn or more is a cut and has no in-between; and until the camera has been
// captured there is nothing to pair with.
static void test_camera_cut_is_the_guests_own_half_turn() {
  ts2::ResidentCameraHistory camera;
  CHECK(!camera.ready());
  CHECK(!camera.continuous());

  ts2::ResidentCameraSample sample;
  sample.position[0] = 100;
  sample.rotation[1] = 0x0100; // the camera eases: 0x10 of a turn in one field
  camera.capture(sample);
  CHECK(camera.ready());
  CHECK(camera.continuous());

  sample.rotation[1] = 0x0110;
  sample.position[0] = 140;
  camera.capture(sample);
  CHECK(camera.continuous());
  CHECK_EQ(camera.previous().position[0], 100);
  CHECK_EQ(camera.current().position[0], 140);

  // A wrap the short way round is not a cut: 0x0FFF -> 0x0011 is 0x12 of a turn forwards.
  sample.rotation[1] = 0x0FFF;
  camera.capture(sample);
  sample.rotation[1] = 0x0011;
  camera.capture(sample);
  CHECK(camera.continuous());

  // Half a turn in one field is not something the game steers, so it is a cut.
  sample.rotation[1] = 0x0011 + 0x800;
  camera.capture(sample);
  CHECK(!camera.continuous());

  // The field after the cut is continuous again: the cut is one field wide, not a latch.
  sample.rotation[1] = 0x0011 + 0x810;
  camera.capture(sample);
  CHECK(camera.continuous());
}

int main() {
  RUN(measured_windows_install_projection_gpu_timeout_and_vsync_owners);
  RUN(gpu_timeout_pair_completes_without_guest_vsync);
  RUN(offset_leaf_preserves_retail_state_and_records_projection);
  RUN(screen_leaf_completes_same_core_projection);
  RUN(visibility_pass_scope_is_its_occurrence_in_the_field);
  RUN(modelled_producer_scope_keys_argument_and_occurrence);
  RUN(camera_cut_is_the_guests_own_half_turn);
  return pt_summary();
}
