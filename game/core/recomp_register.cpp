// recomp_register.cpp — fills the framework↔generated-substrate seam
// (recomp_iface.h) from THIS game's recompiled symbols. This is the ONE file
// that names generated/ symbols; the framework reaches them only through
// psxport_recomp()->field.
//
// Its explicit no-substrate branch compiles in the seam-check target. The real
// registry branch cannot compile until `generated/` exists and is guarded by a
// deliberate #error: the recompiler still needs this game's resident-executable
// seeds (RE-02 in docs/re-frontier.md). RE-03's overlay bases are verified.
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

void ts2_install_recomp() {
#ifdef TS2_HAVE_SUBSTRATE
#error "A substrate now exists, so this file must be written for real: fill in the RecompRegistry \
from generated/overlay_table.h (see spider1/game/core/recomp_register.cpp) and delete this guard."
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
