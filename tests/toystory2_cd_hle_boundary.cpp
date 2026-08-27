// Toy Story 2 stock-libcd ownership boundary. The shipping GameConfig must install the framework's
// synchronous CD owners at the measured retail entries, and their real command/sync handlers must
// complete without reaching the mandatory guest-VSync trap.

#include "cd/stock_libcd_layout.h"
#include "core.h"
#include "game.h"
#include "game_iface.h"
#include "legacy_game_interface.h"
#include "platform_hle.h"
#include "testutil.h"

#include <memory>

namespace {

std::unique_ptr<Game> freshGame() {
  psxport_install_game(&ts2::legacy::measuredConfig, &ts2::legacy::compatibilityHooks);
  auto game = std::make_unique<Game>();
  game->cd.overridesInit();
  game->platform_hle.initBuiltins();
  return game;
}

} // namespace

static void test_measured_stock_libcd_entries_are_native_owned() {
  auto game = freshGame();
  const auto &layout = ts2::cd::kStockLibcdLayout;

  CHECK(game->platform_hle.lookup(layout.command) != nullptr);
  CHECK(game->platform_hle.lookup(layout.sync) != nullptr);
  CHECK(game->platform_hle.lookup(layout.getSector) != nullptr);
  CHECK(game->platform_hle.lookup(layout.read) != nullptr);
  CHECK(game->platform_hle.lookup(layout.readSync) != nullptr);
  CHECK(game->platform_hle.lookup(layout.searchFile) != nullptr);
  CHECK(game->platform_hle.lookup(layout.libraryWindowLo - 4) == nullptr);
  CHECK(game->platform_hle.lookup(layout.libraryWindowHi) == nullptr);
}

static void test_sync_reports_completed_and_clears_result() {
  auto game = freshGame();
  Core &core = game->core;
  const uint32_t result = 0x80010000u;
  for (uint32_t i = 0; i < 8; ++i) {
    core.mem_w8(result + i, 0xA5u);
  }

  core.r[5] = result;
  game->platform_hle.lookup(ts2::cd::kStockLibcdLayout.sync)(&core);

  CHECK_EQ(core.r[2], 2u);
  for (uint32_t i = 0; i < 8; ++i) {
    CHECK_EQ(core.mem_r8(result + i), 0u);
  }
}

static void test_setloc_preserves_guest_bookkeeping_and_native_head_position() {
  auto game = freshGame();
  Core &core = game->core;
  const auto &layout = ts2::cd::kStockLibcdLayout;
  const uint32_t position = 0x80010020u;
  const uint32_t result = 0x80010030u;
  const uint8_t requested[] = {0x00u, 0x02u, 0x10u, 0x00u};
  for (uint32_t i = 0; i < 4; ++i) {
    core.mem_w8(position + i, requested[i]);
  }
  for (uint32_t i = 0; i < 8; ++i) {
    core.mem_w8(result + i, 0xA5u);
  }

  core.r[4] = 0x02u; // CdlSetloc
  core.r[5] = position;
  core.r[6] = result;
  game->platform_hle.lookup(layout.command)(&core);

  CHECK_EQ(core.r[2], 0u);
  CHECK_EQ(game->cd.setloc_lba, 10);
  for (uint32_t i = 0; i < 4; ++i) {
    CHECK_EQ(core.mem_r8(layout.lastPosition + i), requested[i]);
  }
  for (uint32_t i = 0; i < 8; ++i) {
    CHECK_EQ(core.mem_r8(result + i), 0u);
  }
}

int main() {
  RUN(measured_stock_libcd_entries_are_native_owned);
  RUN(sync_reports_completed_and_clears_result);
  RUN(setloc_preserves_guest_bookkeeping_and_native_head_position);
  return pt_summary();
}
