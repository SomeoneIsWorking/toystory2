---
id: C020
kind: claim
status: holds
created: 2026-08-24
tags: overlays,re14,binary-analysis
depends: tools/overlay_map.py#loader_contract, docs/info/exe-identity.txt
reconfirmed: 2026-08-26 21:40:52
verified_at: 2026-08-26 21:40:52
---

## Claim

LEVEL01 target `0x800D12C4` is a true overlay function entry: the entry immediately after the
four-byte module header at slot base `0x800D12C0`, not a coroutine resume or corrupted pointer.

## Evidence

The 21 scannable LEVEL modules begin with a small sequential identifier and code at `+4`. LEVEL01's
first words are a normal MIPS prologue (`27BDFFE8 AFBF0014 AFB00010 8C900000`). The resident program
contains a direct `jal 0x800D12C4` guarded by the level state and five 0x98-stride descriptors store
the same entry. A captured loaded prefix matched the LEVEL01 file byte-for-byte for 13,336 bytes.

## What would falsify it

Identity-matching retail bytes show `+4` is data, the direct call or descriptor values differ, or a
trace shows the target is entered only as a continuation with pre-existing frame state.
