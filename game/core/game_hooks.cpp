// game_hooks.cpp — the Toy Story 2 GameHooks vtable: the behaviour the
// PSX-generic framework calls into. This port owns only the measured field-clock
// lifecycle natively, so the table remains deliberately tiny.
//
// There are exactly two kinds of member here, and the distinction is the point
// (the shape is taken from spider1/game/core/game_hooks.cpp, which learned it
// the hard way):
//
//   NEUTRAL   — the hook asks "what does the GAME's native code contribute
//   here?", and the honest
//               answer while nothing is owned is "nothing". A neutral body is
//               the CORRECT semantic, not a placeholder.
//   FAIL-FAST — the hook is only reachable from a framework path this port has
//   not stood up. Being
//               called means the run wandered into an un-RE'd path, and the
//               only correct response is to say so loudly and abort. A silent
//               stub would let a half-wired path look like it worked, which is
//               the fake-green the porting doc warns about.
//
// bootInit dispatches the RE-01-verified guest main() once a substrate exists;
// registerOverrides installs RE-10's field clock. The guard prevents a future
// config regression from turning into a dispatch to address zero.
#include "cfg.h"
#include "core.h"
#include "game_iface.h"
#include "sync/field_clock.h"
#include <stdlib.h>

// ── boot
// ────────────────────────────────────────────────────────────────────────────────────────
static void ts2_bootInit(Core *c) {
  if (!c->cfg->gameMain) {
    cfg_loge("boot",
             "GameConfig::gameMain is 0, contradicting the RE-01 verifier and "
             "shipping boot group. Refusing to dispatch address 0; run "
             "tools/verify_crt0.py --check.");
    abort();
  }
  cfg_logi("boot", "dispatching guest main() 0x%08X on the recompiled substrate", c->cfg->gameMain);
  rec_dispatch(c, c->cfg->gameMain);
}

// ── neutral
// ─────────────────────────────────────────────────────────────────────────────────────
static void ts2_registerOverrides(Game *) {
  ts2_field_clock_install();
}

static void ts2_renderFadeState(Core *, FadeState *out) {
  out->mode = 0; // 0 == no fade; the present path leaves pixels untouched
  out->r = out->g = out->b = 0;
}

static void ts2_renderBbFrameReset(Core *) {
  // No native billboard records are kept — nothing to reset.
}

static bool ts2_hasNativeHandlerForEntry(Core *, uint32_t) {
  return false;
} // truthfully: none
static int ts2_devAreaCount(Core *) {
  return 0;
} // truthfully: no area index RE'd
static const char *ts2_devAreaName(Core *, int) {
  return "";
} // "" == no sourced name
static bool ts2_devWarpAllowed(Core *) {
  return false;
}

// ── fail-fast
// ───────────────────────────────────────────────────────────────────────────────────
static void unstood_up(const char *what) {
  cfg_loge("hooks",
           "%s was called, but this port has not stood that path up yet. "
           "Reaching it means "
           "the run entered an un-RE'd framework path — see "
           "docs/re-frontier.md. Refusing "
           "to continue with fabricated behaviour.",
           what);
  abort();
}

static void ts2_frameUpdate(Core *) {
  unstood_up("frameUpdate (native frame loop)");
}
static void ts2_drawOTag(Core *, uint32_t) {
  unstood_up("drawOTag (native frame loop)");
}
static int ts2_schedStageBody(Core *, int, void *) {
  unstood_up("schedStageBody (PcScheduler)");
  return 0;
}
static bool ts2_schedFreshEntry(Core *, int, uint32_t, uint32_t) {
  unstood_up("schedFreshEntry (PcScheduler)");
  return false;
}
static void ts2_devWarp(Core *, int, int) {
  unstood_up("devWarp");
}

// DESIGNATED initialisers, deliberately — every hook binds BY NAME, so a field
// added upstream cannot slide this table by one (which, between two hooks of
// the same signature, compiles silently and calls the wrong function). C++20
// requires designators in declaration order; keep them so when adding one.
// Unlisted members are value-initialised to null, so this list reads as the
// exact inventory of what this port has stood up.
static const GameHooks g_ts2_hooks = {
    .frameUpdate = ts2_frameUpdate,
    .drawOTag = ts2_drawOTag,
    .bootInit = ts2_bootInit,
    .schedFreshEntry = ts2_schedFreshEntry,
    .hasNativeHandlerForEntry = ts2_hasNativeHandlerForEntry,
    .registerOverrides = ts2_registerOverrides,
    .renderFadeState = ts2_renderFadeState,
    .renderBbFrameReset = ts2_renderBbFrameReset,
    .devWarp = ts2_devWarp,
    .devAreaCount = ts2_devAreaCount,
    .devAreaName = ts2_devAreaName,
    .devWarpAllowed = ts2_devWarpAllowed,
    .schedStageBody = ts2_schedStageBody,
};

const GameHooks *ts2_game_hooks() {
  return &g_ts2_hooks;
}
