---
id: 24
title: Toy Story 2 widescreen proof used unsupported PSXPORT_WIDE knob
status: investigating
symptom: Capability run set PSXPORT_WIDE=1, startup reported it UNKNOWN and doing nothing, and no aspect setting was loaded, so the run remained 4:3 evidence
tags: widescreen,settings,cvar,validation,evidence
state_items: S010
created: 2026-08-26
updated: 2026-08-27
---

The exact psxport `99a42aa3` capability run cannot prove widescreen. Its startup audit reported
`PSXPORT_WIDE` as UNKNOWN, and the shipping framework has no such CVar or legacy reader. Widescreen is
selected through the video settings file instead: `PSXPORT_SETTINGS` chooses the INI path and
`aspect=1` requests 16:9 (`aspect=0` is 4:3). The run used the default `psxport_settings.ini` and no
settings file/aspect evidence was present.

Evidence: `scratch/logs/capabilities-product-99a42aa3.log` lines 5, 21, and 29; shipping settings
contract in `external/psxport/docs/config.md` and loader in
`external/psxport/runtime/recomp/mods.cpp`.

A future run must point `PSXPORT_SETTINGS` at a controlled INI containing `aspect=1`, select the
runtime-verified GTE product path, and capture direct evidence that the loaded aspect and rendered
projection are wide. Native and fps60 stay unavailable until their separate producers exist. The corrected invocation fragment is
`PSXPORT_SETTINGS=scratch/config/capabilities-wide.ini`, where that ignored file contains
`aspect=1`. `PSXPORT_WIDE` must not appear. Until then S010 remains missing.

### Falsified source candidate (2026-08-27)

The runtime now exposes the framework's title-owned guest-wide policy and stops claiming absent Native
and lerp products. Resident setup supplies the measured 320x240 scanout separately from its 512x240
projection/draw canvas, then applies the shared 16:9 plan (428 presentation, 684 projection/draw,
OFX 342 and a +32 source-centre shift). The hermetic boundary proves those arithmetic results, not a
valid resident VRAM layout.

The controlled real-disc run `scratch/logs/re22-wide-live.log` selected that exact `aspect=1` settings
file and completed all 300 native-owned frames with zero dropped layers and no guest VSync. Player-stage
captures 45 through 300 consistently show large internal vertical black slabs splitting otherwise
coherent Andy's Room gameplay (`scratch/screenshots/re22-wide/contact-sheet.png`). This falsifies the
source candidate.

The root cause is the resident buffer topology, not aspect selection. Retail 0x80039D9C gives each
horizontal parity a 512-pixel draw canvas in fixed 1024-pixel VRAM: one begins at x=0 and the other at
x=512. Expanding either draw environment to 684 pixels crosses that parity's physical half-VRAM
boundary; the presenter then samples wrapped, unused, or texture-atlas columns as if they were the
wide framebuffer. A projection-only boundary cannot detect that collision. Do not compensate with a
crop, stretch, or post-GTE packet replay. True widescreen must come from title-native producers (or a
fully measured relocation of every resident VRAM consumer) whose projection and output surface are
not constrained by the guest's horizontal double-buffer layout.
