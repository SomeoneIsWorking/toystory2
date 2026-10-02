---
id: 40
title: A native movie frame is never visible in a headless capture
status: open
symptom: With a native movie playing, both the `PSXPORT_PRESENT_SHOT_AT` trigger and the control channel's `pshot` return an all-black sink, so no headless capture of this port can show an intro movie
tags: fmv,headless,capture,presentation
created: 2026-08-23
updated: 2026-08-23
---

## Measurement

With a native movie playing and the control channel live (`pshot` answering mid-movie at frames
514/529/544), both captures are entirely black:

| capture | surface | result |
|---|---|---|
| `pshot` | headless sink, 960x720 | 0 non-black columns |
| `shot` | guest framebuffer, 320x240 | 0 non-black columns |
| `PSXPORT_PRESENT_SHOT_AT` | headless sink | `non-black 0/691200 (0.00%)` |

The same is true of a run whose movie plays inside a single host turn, so this is **not** a
consequence of stepping the player one frame per turn.

## Root cause

`gpu_vk_present_image` (`runtime/psx/gpu_vk.cpp`) uploads the decoded RGBA into `s_img_tex` and then
returns early in headless:

```c
if (s_headless) {
  gpu_submit(cmd, "gpu_vk_present_image");
  return;
} // caller PPM-dumps its own rgba headless
```

Only the windowed leg acquires the swapchain and draws `s_img_tex`. The headless sink is written by
the *main* present path (`gpu_present_ex` -> `plan_present` -> the composite), which builds from guest
VRAM — so a native-image present never reaches it. `shot` is black for the same reason: the movie
never touches guest VRAM at all.

The comment states the intended contract ("the caller PPM-dumps its own rgba headless"), and `Fmv`
does not implement it. So the headless leg of the native-image present is unimplemented, not broken.

## Why it was not fixed here

The obvious fix is to composite `s_img_tex` in the headless leg of the main present, which is a
one-place change in `plan_present`/`present_inputs` — but `runtime/psx/gpu_vk.cpp` carries another
agent's uncommitted pane-compositor work, and the render path is the one file where a mistake is
invisible in every other test. It also contradicts the standing preference to reuse the single
existing capture path rather than add a second dump mechanism.

## Consequence

A native movie can be verified structurally — it opens, demuxes, decodes every frame, ends, and its
skip works — but its pixels cannot be verified headlessly. This port's existing 24-bit pixel evidence
comes from an independent decode of the same STR streams rather than from a capture, and that is
still the honest way to check movie pixels today.

## Acceptance

A headless capture taken during a native movie shows the decoded frame, at the same pillarbox the
windowed leg draws it with, from the same `pshot`/`PSXPORT_PRESENT_SHOT_AT` path — no second dump
knob. Then this issue closes and the movie's on-screen appearance becomes checkable like any other
frame.
