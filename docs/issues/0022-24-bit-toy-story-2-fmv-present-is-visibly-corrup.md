---
id: 22
title: 24-bit Toy Story 2 FMV present is visibly corrupted
status: investigating
symptom: Exact-pin real run renders legal and ESRB cards correctly, then present 1500 in 24-bit mode shows duplicated noisy columns and a mostly black frame
tags: render,fmv,24-bit,present,runtime
created: 2026-08-26
updated: 2026-08-26
---

## Observation

An exact-dbdb2baf headless product run captured coherent legal and ESRB screens at presents 300 and 900. After GP1(08) selected 24-bit 256x240 display, present 1500 showed duplicated noisy columns and a mostly black picture. The process remained live and later switched between 24-bit and 15-bit display modes.

Evidence: scratch/screenshots/present_300.ppm, present_900.ppm, present_1500.ppm, and visual montage projection-live-montage.png.

## Next falsifier

Capture the same FMV field through raw guest VRAM and present-stage instruments, plus a software-PSX control. If guest VRAM is already corrupted, trace the FMV upload/24-bit stride; if VRAM is coherent but present is not, isolate the 24-bit display-region sampling/stride conversion. Do not patch the image or force 15-bit mode.
