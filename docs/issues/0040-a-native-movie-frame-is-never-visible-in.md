---
id: 40
title: A native movie frame is never visible in a headless capture
status: closed
symptom: With a native movie playing, both the `PSXPORT_PRESENT_SHOT_AT` trigger and the control channel's `pshot` return an all-black sink, so no headless capture of this port can show an intro movie
tags: fmv,headless,capture,presentation
created: 2026-08-23
closed: 2026-10-03
updated: 2026-10-03
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

## Resolution (2026-10-03) — fixed in psxport, at the present owner

The recorded root cause was real but incomplete. Three things were wrong, and all three had to be fixed
before a capture could show a movie.

**1. The image pipeline did not exist in the headless leg at all.** Not just that `gpu_vk_present_image`
returned early — `s_image_pipe` was created windowed-only (`if (!s_headless)`) against `s_swap_fmt`, so
there was nothing to render with. It is now created in both legs, each against the format of the target
that leg draws into: the swapchain windowed, `PRESENT_IMG_FMT` headless.

**2. The movie now builds into `s_present_img`.** `GpuVkState::build_image_present_image` is the headless
mirror of the windowed swapchain blit: same pipeline, same source texture and sampler, the same
`letterbox(4, 3, ...)` viewport and full-image scissor, the same fragment fade, the same black clear
painting the bars, and it publishes `s_present_viewport` so `measurePresentedContent` has a viewport to
measure against. This follows the precedent `present_screen()` already set ("built into the present
image (so a present shot reads it in either leg)"). No second dump knob was added.

**3. The capture was sampling the picture mid-turn, and that is what actually kept it black.** This was
the part the issue did not reach, and it is the real cause.

A frame can have more than one presenter: the main one (`gpu_present_ex`) and, during a native movie,
`gpu_vk_present_image`. The capture trigger used to fire at the tail of the MAIN presenter. During a movie
the guest draws nothing, so the main presenter's build is `plan.build=yes` over an empty guest-VRAM
composite (measured: 301 of 301 presents), and it runs BEFORE the movie presents — so the trigger read
the empty composite. Black, on every frame of a movie that was decoding and presenting the whole time.

**The trigger moved to the one point where the frame's presentation is over: `FrameLoopShell::step`,
immediately after `stepFrame` returns.** That function already asserts the frame's contract — the
presentation fence advanced exactly once — so it is the frame's boundary by construction, and by then
every presenter of the frame has run. Both presenters now only present, and the capture reads whatever
the turn ended showing, which is the same thing the window shows. `gpu_present_frame_capture()` is the
entry point; the frame number it reports is the same `s_frame` the old tail-of-present trigger used,
because `frame_finalize()` advances that counter inside `gpu_present_ex`, so the numbering does not shift.

**A first attempt at this used a per-frame arm/defer flag pair and was REJECTED.** It had a real defect:
`s_img_presented_since_capture` was cleared only by a capture, so a movie played with no capture
requested left the flag set, and the first ordinary gameplay capture afterwards armed and never fired,
because no image present came again to release it. It also made two presenters coordinate through flags
to express what is really a property of the frame. Both parts are gone: there is no arm, no defer, no
flag, and no second capture path.

### Measured evidence — Toy Story 2 `toy2fmv/acti.str`, sink 960x720 (4:3)

| frame | before | after |
|---|---|---|
| 140 | `non-black 0/691200 (0.00%)` | 182122 (26.35%) |
| 220 | 0.00% | 691053 (99.98%) |

The 4:3 figures are **identical** to the arm/defer version's, so the end-of-frame capture reads exactly
the picture the flag hand-off did — with none of the flags. The frames were opened and looked at, not
trusted on the metric: frame 140 is Buzz's craft mid-fade, frame 220 is the TOY STORY 2 logo at full
brightness.

16:9 (sink 1280x720), same route and frames: 182122/921600 and 691053/921600 — the same picture
**pillarboxed 4:3** with black bars at sink x 0..160 and 1120..1280, identical geometry to the windowed
leg, which letterboxes the image pipeline 4:3 in both legs.

### The regression this design is actually for

A run with `replays/toystory2_player_andys_house_v2.pad`, **five movies played and no capture requested
during any of them**, and a single `PSXPORT_PRESENT_SHOT_AT=1900`:

```
wrote scratch/screenshots/present_1900.png (960x720 headless sink) non-black 662571/691200 (95.90%)
```

That is the case the flag design got wrong. The frame was opened and is Buzz standing in Andy's House.
A flag-latched on a movie and cleared only by a capture could not have produced it.

`uv run --frozen python tools/verify.py --jobs 6`: PASS, 7/7 CTest, execution-boundary clean.
`clang-format` clean on every file touched.

### Two measurement errors worth recording

- **`PSXPORT_DEBUG` does not reach the product through the shell.** `tools/headless_run.py` sets
  `PSXPORT_DEBUG` from its own `--debug` flag, overwriting the environment. Three runs reported "0 debug
  lines" for channels I had not touched, which I first read as "the code never runs". It is the tool's
  flag: `--debug <channels>`.
- **Reading back `s_present_img` from inside `build_image_present_image` measures the PREVIOUS turn.**
  That function records into a command buffer the caller has not submitted yet, so a download issued
  there sees the last submitted picture. It produced a convincing rising curve of real content that was
  one frame stale, and it would have "confirmed" a fix that was not working. A capture is only evidence
  once the pass that wrote it has been submitted — which is exactly what `present_shot` does.
