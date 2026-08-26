---
id: C022
kind: claim
status: holds
created: 2026-08-26
tags: input,boot
depends: game/core/game_config.cpp#kPadSlot0Buffer, game/sync/field_clock.cpp#field_turn
---

## Claim

Toy Story 2 host input is delivered to the retail pad buffers at each game-owned field turn

## Evidence

RE-06/I018/issue #19. Exact retail instructions pass 0x800CF8A0/0x800CF8C8 to 0x800971D8, which registers them at 0x800A3E98/0x800A3F88 with 0xF0 context stride; consumer 0x8003AC58 reads slot 0. tools/verify_pad_buffers.py matches all five shipping bindings and passes 5/5 classes. Bounded headless PSXPORT_FORCE_BUTTONS=BFFF witness writes 0xBF on fields 1..8, 0xFF on 9..32, then 0xBF again at 33.

## What would falsify it

A retail call/registration/consumer mismatch, failure of verify_pad_buffers.py, removal of field_turn Pad::serviceFrame, or a live force pulse that no longer produces both 0xBF and 0xFF at 0x800CF8A3 falsifies this.
