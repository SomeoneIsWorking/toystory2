---
id: 27
title: Toy Story 2 FMV overlay owns a blocking guest movie loop
status: investigating
symptom: FMV/FMV.BIN function 0x800D7088 decodes and presents every STR frame inside one guest call and directly calls linked VSync 0x80088628 once per movie frame
state_items: S003,S007
tags: frame-loop,vsync,fmv
created: 2026-08-27
updated: 2026-08-27
---

## Root cause

Retail `0x800D7088` performs the whole STR open/demux/MDEC/upload/display loop and calls the linked
`VSync(0)` at return PC `0x800D7590` once per movie frame. Its only direct caller is `0x800D6628`.
One guest call therefore spans an unbounded presentation loop, which no one-turn finite budget can
hold; the native frame owner must take the loop, one display field per step, and never let the guest
VSync succeed.

## What is owed

`psx::fmv::play` is a native blocking movie owner, but it returns a frame count and does not preserve
the guest contract's playback-mode skip result, so it cannot simply be substituted. The FMV loop owner
must present the skip outcome retail returns, with one display field per host frame.