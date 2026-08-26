---
id: 22
title: 24-bit Toy Story 2 FMV present is visibly corrupted
status: investigating
symptom: Exact-pin real run renders legal and ESRB cards correctly, then present 1500 in 24-bit mode shows duplicated noisy columns and a mostly black frame
tags: render,fmv,24-bit,present,runtime
state_items: S007
created: 2026-08-26
updated: 2026-08-26
---

## Observation

An exact-dbdb2baf headless product run captured coherent legal and ESRB screens at presents 300 and 900. After GP1(08) selected 24-bit 256x240 display, present 1500 showed duplicated noisy columns and a mostly black picture. The process remained live and later switched between 24-bit and 15-bit display modes.

Evidence: scratch/screenshots/present_300.ppm, present_900.ppm, present_1500.ppm, and visual montage projection-live-montage.png.

## Static classification

The retained captures are final present-stage images; there is no synchronized raw guest-VRAM dump
and no software-PSX control for the same movie field. The framework CPU VRAM-shot conversion and the
present fragment shader both use the same rule (`display_x * 2 + local_x * 3` bytes), so comparing
those two paths would not independently falsify a shared 24-bit stride error.

The identity-backed FMV overlay does not support a game-side width patch. `0x800D7088` selects 24-bit
MDEC output when its depth argument is 3, allocates `depth << 13` output bytes, and drives the measured
MDEC DMA wrappers at `0x800D9914`/`0x800D9990`. Its completion callback `0x800D6980` uploads one
16-pixel strip at a time through retail `LoadImage` wrapper `0x80085D18`: 24 VRAM halfwords by 240
rows, advancing 24 halfwords until the double-buffered image rectangle spans 480 halfwords (320 RGB
pixels). DMA block counts are exact `0x20`-word multiples. This geometry is internally coherent.

The remaining static coverage gap is at the platform boundary: psxport's guest-visible MDEC pump has a
bit-identical direct-versus-pump differential for varied synthetic 16-bit output, but no equivalent
24-bit control. Static code cannot determine whether the corrupt field was already present in source
VRAM or introduced while sampling it.

An exact-`99a42aa3` capability run later remained CPU-active while disc activity advanced through LBA
15660 and the GPU repeatedly toggled between 24-bit 256x240 and 15-bit 320x240 display modes. That is
continued-execution evidence only: the run retained no synchronized raw VRAM, final present, or
software control, so it neither proves coherence nor narrows this issue's cause.

## Next falsifier

Capture one synchronized field through (1) the raw 1024x512 two-byte VRAM texture before presentation,
(2) final presentation, and (3) a software-PSX control. Decode the raw bytes with a deliberately
independent small oracle rather than the shipping shader/CPU-shot formula. In parallel, extend the
framework's existing varied MDEC direct-versus-guest-pump differential to 24-bit output and prove that
its discriminator fails when per-word scatter is removed. If raw VRAM is already corrupted, trace
MDEC DMA placement and the strip upload; if raw VRAM is coherent but final presentation is not,
isolate display-region sampling. Do not patch the image or force 15-bit mode.
