// The level-start card override's decision, and nothing else.
//
// FUN_8007C278 loads and presents `gfx\loading.raw` exactly when its own demo guard rewrites the
// requested graphic to id 0: the guard reads `if ((param_2 != 0) && (param_2 != 0x7b)) param_1 = 0;`
// and DAT_800A120C is the attract/demo flag. Getting this wrong in either direction is a real defect —
// suppressing a level's own title graphic, or letting the loading card through — so the guard's two
// exemptions are pinned here rather than left to the decompilation staying true.

#include "boot/level_start_presentation.h"

#include "testutil.h"

#include <cstdio>

namespace {

void test_demo_mode_trips_the_guard(void) {
  CHECK(ts2::demoGuardForcesLoadingCard(1));
}

void test_the_exempt_mode_shows_the_levels_own_graphic(void) {
  CHECK(!ts2::demoGuardForcesLoadingCard(0x7B));
}

void test_an_ordinary_level_start_shows_the_levels_own_graphic(void) {
  CHECK(!ts2::demoGuardForcesLoadingCard(0));
}

void test_only_those_two_exemptions_exist(void) {
  // Every mode other than 0 and 0x7B trips the guest's guard, and only those two do not.
  for (int mode = -3; mode <= 8; ++mode) {
    CHECK_EQ(ts2::demoGuardForcesLoadingCard(mode), mode != 0);
  }
  CHECK(!ts2::demoGuardForcesLoadingCard(0x7B));
  CHECK(!ts2::demoGuardForcesLoadingCard(0));
}

} // namespace

int main() {
  RUN(demo_mode_trips_the_guard);
  RUN(the_exempt_mode_shows_the_levels_own_graphic);
  RUN(an_ordinary_level_start_shows_the_levels_own_graphic);
  RUN(only_those_two_exemptions_exist);
  return pt_summary();
}
