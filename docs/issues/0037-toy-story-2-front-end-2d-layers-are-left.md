---
id: 37
title: Toy Story 2 front-end 2D layers are left-anchored in the 16:9 leg
status: open
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

## Next step

Find the owner that places this layer. It is not one of the resident seams already widened
(`PutDrawEnv` 0x80086BD0, the object cull 0x80027AF0, the screen-rect publisher 0x80010000 — all of
which are gated on the resident canvas being active, and the front end is not the resident leg), and
it is not the framework's centring HUD mapping (`ws_2d_local_x`, `x + (ww - native) / 2`) either,
since a centred element would land at column (684-320)/2 = 182, not 0. The candidates are the MDEC /
movie blit the front end uses for this card and whatever front-end 2D screen rect the guest publishes
for it. Whichever it is, it is a real owner to widen, not a special case to add for this input.