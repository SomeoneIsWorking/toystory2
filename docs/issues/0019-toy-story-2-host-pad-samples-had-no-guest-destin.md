---
id: 19
title: Toy Story 2 host pad samples had no guest destination
status: resolved
symptom: Field clock calls Pad::serviceFrame, but every GameConfig pad destination is zero, so host keyboard/controller input cannot reach the guest packet decoder
tags: input,pad,re06,boot,root-cause
created: 2026-08-26
updated: 2026-08-26
---

## Classified cause

The title already sampled input at every measured host field, but `Pad::serviceFrame` skipped both
slots because `padSlot0Buf`, `padSlot1Buf`, and the driver pointer table were all zero. Retail
`0x8003EEF0` calls linked pad init `0x800971D8` with `0x800CF8A0` / `0x800CF8C8`; that driver registers
the pointers at `0x800A3E98` / `0x800A3F88` in `0xF0`-byte contexts. Independent decoder
`0x8003AC58` reads slot 0 directly.

## Resolution

`game/core/game_config.cpp` now supplies those binary-derived facts. `tools/verify_pad_buffers.py`
exact-checks the unique call, registration stores, stride, consumer and shipping bindings, with 5/5
positive/negative/refusal classes. A bounded headless force pulse writes `0xBF` (Cross active-low)
then `0xFF` into the measured buffer. This resolution covered delivery only. A later RE-17 product
run proved gameplay response and visible movement; issue #20 owns the remaining replay-determinism
gap.
