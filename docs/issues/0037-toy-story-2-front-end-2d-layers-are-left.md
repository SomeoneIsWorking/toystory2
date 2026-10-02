---
id: 37
title: Toy Story 2 front-end 2D layers are left-anchored in the 16:9 leg
status: resolved
symptom: at 16:9 the Level-1 "PRESS X" card keeps its authored 320-column width, sits at the canvas origin and leaves the rest of the widened front-end frame black, with unwritten VRAM in the extreme edge columns; at 4:3 the same card is the full frame
state_items: S010
tags: widescreen,front-end,2d,mdec,fmv
created: 2026-10-03
updated: 2026-10-03
---

## Measured

Same route, same field, same binary (`scratch/s010`):

| capture | aspect | sink | card non-black extent |
|---|---|---|---|
| `m690.png` | 16:9 | 1280x720 | x 0..600 of 1280 (47%) |
| `n690.png` | 4:3 | 960x720 | x 0..959 (full frame) |

The front-end canvas is 320 columns wide at 4:3 (`960/320 = 3.0`) and 684 columns wide at 16:9
(`1280/684 = 1.872`); the card is 320 columns wide in both, so the wide leg keeps its authored size
and anchors it at the canvas origin instead of centring it in the widened canvas. The widened
columns the guest never draws show black plus a 1-2 px strip of unwritten VRAM at the extreme left
and right sink columns (`m690_bl.png`, `m690_br.png`).

The intro movies on the same route are NOT affected: `f100.png` (16:9, x 160..651) and `nm100.png`
(4:3, x 0..491) are the same 491 px image, centred in the wider sink, which is the correct behaviour
for a centred element. So the two front-end presentation paths differ.

## Why it matters

The widescreen rule is that an element anchored to the centre stays centred and nothing stretches. A
full-frame front-end card is centred content; leaving 53% of the widened frame black with garbage in
the edge columns is a visible defect in the shipped 16:9 picture, and it is a title-observable one
(the front end is where the player spends the first thirty seconds).

## Root cause, measured

`--debug wide,ires` on the same route. The front end runs the guest's own 320x240 display
(`[wide] native picture: native_width=320 render_width=320`), and the render width is what the host
presents. It became 684 the moment the guest published its first 512-wide screen — the Level map,
which is still front end — and stayed 684 for the rest of the run, including the card, because:

1. **`retire()` was a flag, not a publication.** `ResidentWidescreenProjection::active_` gates the
   title's own seams, but the presenter reads the LATCHED plan (`GameRuntime::guestDisplay.plan()`),
   which nothing ever un-latched. So a widening meant for one leg stayed in force for every later
   frame. psxport now has the inverse publication beside the latch,
   `gpu_vk_unlatch_guest_projection`, built by the same `guest_projection_plan` from the guest's
   CURRENT display mode, and the title's retire path calls it.
2. **The width test was not the leg test.** `s_disp_w == canvas width` is true for the Level map as
   well as for the room, and the resident widening went with it: the guest laid the map preview out
   beside a panel it had not reflowed, so the menu stopped filling its own frame. The host knows which
   leg it is presenting, so `syncToGuestDisplay(core, residentFrame)` now requires both, and the
   frame driver answers it from its own phase (`residentSetup`/`resident`).

## Result, measured

Every front-end screen at 16:9 now presents at exactly the width it was authored at, centred with
equal bars, and 4:3 is byte-identical before and after:

| field | screen | 16:9 non-black extent | 4:3 |
|---|---|---|---|
| 100 | intro movie 1 | x 160..651 (492 px) | x 0..491 — same picture, shifted by exactly 160 |
| 200 | intro movie 2 | x 592..675 (84 px) | x 432..515 — shifted by 160 |
| 260 | intro movie 3 | x 184..948 (765 px) | x 24..788 — shifted by 160 |
| 340 | movie 4 / title | x 160..1119 (960 px) | full frame |
| 420, 470 | title screen | x 160..1119 (960 px) | full frame |
| 600, 640 | Level map | x 160..1119 (960 px) | full frame |
| 690, 760, 790 | Level card | x 160..1119 (960 px) | full frame |

The bars are clean black — no unwritten VRAM, nothing stretched, the map preview the widening had
exposed is back inside the authored frame. Resident gameplay at 16:9 is byte-identical before and
after (fields 900, 1000, 1060, 1100), and 4:3 gameplay is byte-identical too (field 1060), so the
widening the room needs is untouched.