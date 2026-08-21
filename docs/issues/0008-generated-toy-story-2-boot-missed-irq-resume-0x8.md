---
id: 8
title: Generated Toy Story 2 boot missed IRQ resume 0x80088A2C
status: resolved
symptom: first live generated boot aborts at recomp-MISS 0x80088A2C from irqPoll while CD command waits
tags: recomp,boot,irq,reentry
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

`game/recomp_seeds.json` omitted the IRQ callback resume because the game stores
`0x80088A2C` in guest RAM and invokes it through the runtime interrupt chain. Direct `jal` and pointer
scans therefore cannot discover it. The exact instructions show a mid-function resume, not an ordinary
function entry: it consumes live `v0`, has no prologue, and reaches the containing function's shared
stack epilogue.

## What was tried / dead ends

Ghidra's static reference database reports zero callers and no containing function. Forcing a function
at `0x80088A2C` only labels that address; it does not establish an ordinary call edge or entry prologue.
Under the earlier emitter, listing it only as `main_reentry` preserved fall-through but did not make the
address a discovery root. Duplicating it in `main` worked around that emitter limitation but created two
apparent authorities for one measured entry class.

## Resolution

### Note (2026-08-21)
Live fail-fast recorded guest RAM [0x8009ECD0]=0x80088A2C and sp=0x8009FCB0. Ghidra xref found zero static references and no containing function; forced decompile created FUN_80088A2C. Exact words show no prologue, a branch on live v0 at entry, and shared epilogue restoring ra/s0 then adding 0x18 to sp, proving runtime mid-function re-entry rather than an ordinary function.

### Resolution (2026-08-21)
psxport 692b9b20 makes `main_reentry` an authoritative discovery root and emits its
wrapper/body/dispatch directly. `0x80088A2C` now appears only in that semantically correct list;
regeneration still declares and dispatches `func_80088A2C` with the same 321-root/864-function
denominator.
