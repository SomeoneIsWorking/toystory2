# Project goals

Toy Story 2 is a PC-native port of the USA `SLUS_008.93` release. These goals record durable product
outcomes. Factual delivery coverage lives in `docs/project-state.md`, atomic work in `docs/issues/`,
ownership and placement in `docs/codemap.md`, and the ordered binary-evidence chain in
`docs/re-frontier.md`.

## G001 — Faithful playable PC product

Deliver the retail game as a portable desktop product built from user-supplied disc data, with its
gameplay, timing, input, audio, saves, movies, menus, and transitions intact.

Success means `./run.sh` provisions and starts the intended product from a fresh clone, representative
front-end and gameplay sessions are inspected as well as tested, and supported player choices reflect
capabilities the title actually implements.

Contributing state: S001, S002, S003, S004, S005, S006, S007, S012, S013.

## G002 — True widescreen

Extend the game's own projection, visibility, edge coverage, and 2D layout to wide aspect ratios while
retaining a faithful 4:3 path.

Success requires additional correctly composed world content rather than host stretching or cropping.
Matched title and gameplay captures must preserve central scale, culling, and authored UI placement.

Contributing state: S008, S009, S010.

## G003 — Native and interpolated 60fps presentation

Render from game-owned scene state through title-owned native producers and present interpolated frames
between simulation ticks at the player-facing 60fps cadence.

Success requires every visible layer to have an attributed producer, stable previous/current camera and
object state, and representative real-game comparisons against retained guest behavior. Interpolation
does not decorate final GP0 primitives, and a native renderer is not considered implemented merely
because the framework can consume guest packets.

Contributing state: S008, S009, S011.

## G004 — Loading removal

Remove storage latency and loading-only waits from every load the game performs, without changing
unrelated scripted timing or faking completion. Loading runs asynchronously and the product goes
straight to the next real presentation.

Success requires each measured load operation to deliver the same payload and terminal state as
retail while omitting its loading-only presentation. Logo screens accept Start/Cross through the
title's recovered cancellation route (or a purpose-built skip establishing the same lifecycle,
resource, and state invariants). Authored transition cutscenes are presentation, not loading, and
remain.

Faster simulation, bypassed lifecycle callbacks, written phase/timer/scene words, and presentation
tricks that hide a wait are not implementations of this goal. Loading removal is suppressed under
oracle comparison.

Contributing state: S015.

## Constraints and non-goals

- Guest instructions not deliberately owned by verified native subsystems execute from the
  user-supplied image through psxport's runtime dynarec; only classified bounded fallback is permitted, and no interpreter-only gameplay or static emitted
  guest corpus is permitted.
- An independent emulator or separately built diagnostic oracle remains available for differential
  verification while native ownership grows.
- There is no decompilation or matching symbol map for this release. Guest addresses and behavior must
  remain grounded in reproducible evidence from the verified executable and modules.
- Missing native producers, widescreen policy, or interpolation state remain explicit. Presentation
  workarounds and guessed guest constants are not substitutes.
