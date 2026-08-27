// Toy Story 2 projection-publication boundary: the title's measured libgte addresses must install
// the shared faithful leaves, mutate CPU/GTE state exactly like retail, and record authored camera
// projection on the same Core. Hermetic: no disc, generated substrate, GPU, window, or game loop.

#include "core.h"
#include "game.h"
#include "game_iface.h"
#include "hw_bind.h"
#include "legacy_game_interface.h"
#include "platform_hle.h"
#include "testutil.h"

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
  psxport_install_game(&ts2::legacy::measuredConfig, &ts2::legacy::compatibilityHooks);
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

int main() {
  RUN(measured_windows_install_projection_gpu_timeout_and_vsync_owners);
  RUN(gpu_timeout_pair_completes_without_guest_vsync);
  RUN(offset_leaf_preserves_retail_state_and_records_projection);
  RUN(screen_leaf_completes_same_core_projection);
  return pt_summary();
}
