---
id: C007
kind: claim
status: holds
created: 2026-08-12
tags: state,honesty
depends: game/core/game_config.cpp, game/recomp_seeds.json, docs/re-frontier.md
---

## Claim

This port has NO recompiled substrate, NO native producer, NO native renderer body and NO reverse-engineered guest address: every GameConfig guest field is 0 and every re-frontier step is todo or blocked

## Evidence

VERIFIED 2026-08-12 by inspection of the tree as created. game/core/game_config.cpp: every guest-address field is 0; the only nonzero values are port facts and not RE (discEnvVar PSXPORT_TS2_DISC, cardEnvVar/cardDefaultPath, preserveVramBackdrop 1, paceQuota 1, windowTitle). The PS-EXE header facts (entry 0x80082D60, text 0x80010000+0x91800, the two disagreeing stack values) and the fitted overlay base 0x800D1000 are recorded as static constexpr constants with static_asserts and are deliberately NOT wired into the struct, because the framework consumes the boot group AS A GROUP and an overlay is keyed BY its load address. game/recomp_seeds.json is empty in all five keys. generated/ does not exist, so cmake/toystory2_port.cmake does not configure the port target at all and prints a STATUS saying why. game/core/recomp_register.cpp is unwritten and errors under TS2_HAVE_SUBSTRATE. game/core/game_hooks.cpp is neutral bodies where nothing-is-owned is the correct semantic and fail-fast aborts for every framework path this port has not stood up, with bootInit refusing rather than dispatching gameMain == 0. docs/re-frontier.md: re_frontier.py stats reports 10 steps, 2 todo and 8 blocked, ZERO re-verified, ZERO hacks. WHY THE CLAIM EXISTS AT ALL: a tree with this much prose, four working instruments and five measured claims reads like progress at a glance. It is not progress on the PORT — every measured fact in docs/info/claims is about the DISC or the SUPPLY. The distinction is the thing a later session is most likely to lose.

## What would falsify it

generated/rec_sources.cmake existing (a substrate has been emitted), or a toystory2_port binary existing, or any nonzero guest address in game/core/game_config.cpp, or any docs/re-frontier.md entry reaching re-verified or re-partial. Any of those means this claim is stale and must be rewritten rather than left to read as current
