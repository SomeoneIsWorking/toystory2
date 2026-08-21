// recomp_register.cpp — fills the framework↔generated-substrate seam
// (recomp_iface.h) from THIS game's recompiled symbols. This is the ONE file
// that names generated/ symbols; the framework reaches them only through
// psxport_recomp()->field.
//
// Its explicit no-substrate branch compiles in the seam-check target. The real
// branch is built only after tools/recomp_substrate.py has identity-checked and
// emitted the executable plus all 21 RE-03-verified code modules.
//
// When the substrate lands, this becomes the shape
// spider1/game/core/recomp_register.cpp has: a designated-initialiser
// RecompRegistry naming main_dispatch, rec_func_index, the overlay table and
// shard_set_override from generated/overlay_table.h. It is left UNWRITTEN
// rather than written against guessed symbol names, because a plausible-looking
// wrong registry is exactly the kind of thing that reads as a framework bug
// later.
#include "cfg.h"
#include "core.h"
#include "recomp_iface.h"
#include <stdlib.h>

#ifdef TS2_HAVE_SUBSTRATE
#include "overlay_table.h"

extern void shard_set_override(uint32_t, void (*)(Core *));

static const RecompRegistry kTs2Recomp = {
    .main_dispatch = main_dispatch,
    .rec_func_index = rec_func_index,
    .overlays = g_rec_overlays,
    .overlay_count = g_rec_overlay_count,
    .shard_set_override = shard_set_override,
};
#endif

void ts2_install_recomp() {
#ifdef TS2_HAVE_SUBSTRATE
  psxport_install_recomp(&kTs2Recomp);
#else
  // No substrate: install nothing. NOT silent — a run that gets here has no
  // recompiled code to dispatch to, and finding that out at the first
  // rec_dispatch would blame the wrong thing.
  cfg_loge("recomp",
           "no recompiled substrate is registered: generated/ has "
           "never been emitted for "
           "this game (RE-02 resident-executable seeds — "
           "docs/re-frontier.md). Nothing can "
           "execute. Refusing to continue.");
  abort();
#endif
}
