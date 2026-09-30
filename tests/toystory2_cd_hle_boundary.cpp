// Toy Story 2 stock-libcd ownership boundary. The shipping GameConfig must install the framework's
// synchronous CD owners at the measured retail entries, and their real command/sync handlers must
// complete without reaching the mandatory guest-VSync trap.

#include "cd/stock_libcd_layout.h"
#include "cd/str_completion_layout.h"
#include "core.h"
#include "dma_irq.h"
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

// A stand-in for a measured BIOS table address. The stride is the framework's contract, so the test
// needs a plausible non-zero base and nothing more; asserting the arithmetic keeps the negative case
// honest without pretending this title owns such a table.
constexpr uint32_t ring_table_probe() {
  return 0x801FFF70u;
}

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

// The STR ring facts the FMV wait depends on. These are not exercised by the CD owners above -- the
// ring is guest code -- so what this can honestly assert is their SHAPE and their RELATIONSHIP to
// the natively owned window. Each address is re-derived from the retail bytes by
// tools/verify_str_completion.py; a wrong one here would send the next session after a word nothing
// writes, which is the mistake this file's own history already contains twice.
static void test_str_completion_facts_are_distinct_and_in_guest_ram() {
  const auto &ring = ts2::cd::kStrCompletionLayout;
  const uint32_t words[] = {ring.ringBaseSlot, ring.ringHeadSlot, ring.postGuardA, ring.postGuardB, ring.userCallback};
  for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
    CHECK(words[i] >= 0x80000000u);
    CHECK(words[i] < 0x80200000u);
    for (size_t j = i + 1; j < sizeof(words) / sizeof(words[0]); ++j) {
      CHECK(words[i] != words[j]);
    }
  }
  // The two sides of the ring must agree on the stride and on what "done" means, or the post
  // satisfies a different slot than the pop reads.
  CHECK_EQ(ring.entryStride, 32u);
  CHECK_EQ(ring.completeState, 2u);
  // The post is reached from a site after it, and the three code entries are distinct. Their
  // relative order carries nothing: the pop (0x800940F4) sits ABOVE the post (0x80093E88), so an
  // ordering assertion here would be an assumption about layout rather than a claim about the ring.
  CHECK(ring.postCallSite > ring.postEntry);
  CHECK(ring.popEntry != ring.postEntry);
  CHECK(ring.postCallSite != ring.postEntry);
  CHECK(ring.postCallSite != ring.popEntry);
}

// The ring code must stay GUEST code. If the native window were widened over 0x80093E88 or
// 0x800940F4 the pop and the post would stop running, and the FMV wait would fail differently and
// much later -- so this is the regression that matters, not the addresses themselves.
static void test_str_ring_code_is_outside_the_natively_owned_window() {
  auto game = freshGame();
  const auto &layout = ts2::cd::kStockLibcdLayout;
  const auto &ring = ts2::cd::kStrCompletionLayout;
  for (uint32_t entry : {ring.popEntry, ring.postEntry, ring.postCallSite}) {
    CHECK(game->platform_hle.lookup(entry) == nullptr);
    CHECK(entry >= layout.libraryWindowLo);
    CHECK(entry > layout.libraryWindowHi);
  }
}

// The title declares NO per-channel DMA callback table, and the reason is measured (see
// game/core/game_config.cpp): the SDK's DMACallback is a BIOS B0-vector entry and the table is the
// BIOS's. The negative case is the point -- with a zero base the framework must yield NO slot for
// any channel, never a slot pointing at something arbitrary, and a non-zero base must address the
// per-channel stride the BIOS uses. Both halves are asserted, because a census that can only say
// "zero" cannot tell those apart from a broken read.
static void test_no_dma_callback_table_is_declared_and_yields_no_slot() {
  CHECK_EQ(ts2::legacy::measuredConfig.dmaCallbackTable, 0u);
  for (int channel = 0; channel < 7; ++channel) {
    CHECK_EQ(dma_callback_slot(ts2::legacy::measuredConfig.dmaCallbackTable, channel), 0u);
  }
  // The positive half: a real table's slots are 4 bytes apart, one per channel, and a channel
  // outside the table never addresses into the next structure.
  const uint32_t table = ring_table_probe();
  CHECK_EQ(dma_callback_slot(table, 0), table);
  CHECK_EQ(dma_callback_slot(table, 3), table + 12u);
  CHECK_EQ(dma_callback_slot(0, 3), 0u);
}

int main() {
  RUN(measured_stock_libcd_entries_are_native_owned);
  RUN(sync_reports_completed_and_clears_result);
  RUN(setloc_preserves_guest_bookkeeping_and_native_head_position);
  RUN(str_completion_facts_are_distinct_and_in_guest_ram);
  RUN(str_ring_code_is_outside_the_natively_owned_window);
  RUN(no_dma_callback_table_is_declared_and_yields_no_slot);
  return pt_summary();
}
