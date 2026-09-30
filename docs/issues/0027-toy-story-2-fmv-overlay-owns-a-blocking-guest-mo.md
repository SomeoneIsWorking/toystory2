---
id: 27
title: Toy Story 2 FMV overlay owns a blocking guest movie loop
status: investigating
symptom: FMV/FMV.BIN function 0x800D7088 decodes and presents every STR frame inside one guest call and directly calls linked VSync 0x80088628 once per movie frame
state_items: S003,S007
tags: frame-loop,vsync,fmv,re18
created: 2026-08-27
updated: 2026-09-30
---

## Root cause


## What was tried / dead ends


## Resolution

### Note (2026-08-27)
Ghidra decompilation and exact retail instructions show that 0x800D7088 performs the entire STR open/demux/MDEC/upload/display loop and calls VSync at return PC 0x800D7590 once per movie frame. Its only direct caller is 0x800D6628. psxport `Fmv::play` is a native blocking movie owner, but it returns frame count and does not expose the guest contract's playback-mode skip result; do not substitute it until the title seam preserves that outcome.

### Live boundary (2026-09-12)

After the exact LEVEL00 RAW transaction returned through bounded Lightrec execution, the next strict
front-end call reached FMV entry `0x800D6628` and refused after 46 cycles: no active code-image identity
covered that address. The title now has a shared-slot observer that authenticates the FMV source and
transferred bytes, retires MEMORY, invalidates stale translations, and publishes an FMV generation.
That owner passes synthetic identity transitions and a bounded retail run published authenticated
FMV generation 4. The run next logged `CdRead(1 sectors) with NO Setloc` and exhausted a strict guest
call at `0x800940F4`; their relationship is not yet established. This precedes the independent
movie-loop ownership above; the run completed no frame.

### CD result path (2026-09-12, static and bounded live)

The first FMV `CdRead(1)` call at `0x800D879C` follows `FUN_80090850(1, 0x800EB758)`.
That boot function builds a track table by sending GetTN (`0x13`) and GetTD (`0x14`) through
`FUN_80090FA4`, which directly calls the stock command entry `0x80091DE4` and then waits at
`0x80091898` with the same result pointer. The FMV caller takes the first track's returned
BCD minute/second bytes from `0x800EB75D/E`, computes its sector base, adds `0x10`, converts
that sector to MSF in `0x800D8594`, and calls `0x80090E78(0x15, MSF)`. The latter sends
Setloc (`0x02`) before SeekL (`0x15`) when its non-null position pointer is supplied.

The configured native `cd_command_stock_sync` and `cd_sync_stock_sync` both zero their result
buffers; neither implements GetTN/GetTD output. A bounded debugger run against the existing
Clang binary (SHA-256 `2ec3355455ca3f24a0efa2e1d9c3f5e4344e974e5b15036ca5a427f11b039461`,
build receipt psxport `8b210329`) reached authenticated FMV generation 4 and observed one
GetTN plus two GetTD commands, all returning `00 00 00 00`. The guest then **did** send Setloc
with `00 00 16 01` (BCD MSF `00:00:16`), which changed `Cd::setloc_lba` from 1141 to -1;
SeekL followed, and the exact FMV `CdRead(1, mode 0x80)` caller `0x800D87A4` saw LBA -1.
The stop was the seventh stock read after six earlier reads with valid Setloc positions;
30 native commands had included seven Setloc, seven SeekL, one GetTN, and two GetTD calls.
The same probe therefore demonstrated both outcomes. The “NO Setloc” diagnostic conflates
invalid position with absent command.

The missing TOC command/completion result lifecycle was the CD refusal's root cause. Shared
psxport now derives GetTN/GetTD from `DiscState` track metadata and retains that result through
CdSync; filling only the command result would have been erased by the old sync handler.

A single bounded authentic-disc run of the Toy Story 2 Clang binary (SHA-256
`e985970d8fe355271d2ac437d09c878155caaec7e6dea3570b3cdce3cd81d406`, build receipt
psxport `9c7dd098`) reached authenticated FMV generation 4. Command **and CdSync** returned
`02 01 01 00` for GetTN, `02 59 51 00` for GetTD(0) lead-out, and `02 00 02 00` for GetTD(1).
The guest sent Setloc `00 02 16 01`, changing LBA 1141 to 16. The first FMV `CdRead(1)` at
return PC `0x800D87A4` returned `v0=1` and advanced LBA 16 to 17. Two further FMV read entries
were reached (`0x800D886C`, `0x800D8C0C`), and the guest switched display to 24-bit.
The trace counted 9 native reads (6 pre-FMV, 3 FMV), 10 Setloc, 10 SeekL, 1 GetTN, 2 GetTD,
3 TOC CdSync, and 39 native commands. It then stopped at a distinct strict frame-driver
budget exhaustion, PC `0x80094158` after 564,492 cycles. That stop's cause is unclassified;
no frame completed and no whole-run fallback or movie-playback claim follows from this run.

### Classification of the `0x80094158` exit (2026-09-30) — an UNDELIVERED EVENT

**Reproduced** on the shipping binary built from this tree's source (`build/bin/toystory2_port`,
SHA-256 `313b04bc73ef4c29…`, receipt psxport `9c7dd098`), headless and silent through
`heavy.py --kind run`, `PSXPORT_DEBUG=cd,cdirq,irq,dmairq,cdc`. Same stop, byte for byte:
`frame driver required a completed guest call, but execution exited as budget-exhausted at
0x80094158 after 564492 cycles`, 564,492 = the one-turn budget 33,868,800/60 = 564,480 plus
12 cycles of call-site accounting. So the number is a whole display field, spent.

**WHICH CALL.** A gdb backtrace at the abort names it: `ts2::callGuestToReturn` →
`ts2::(anonymous namespace)::callGuest (address=2147741260, a0=2, a1=0)`, i.e.
`0x8003EE4C(2, 0)` = `kMemoryStatus` at `toystory2_frame_driver.cpp:143`, inside
`restartColdFrontEnd` on logic frame 9. Guest state at the exit: `pc=0x80094158`,
**`ra=0x800D6DEC`**.

**WHAT `0x80094158` IS — NOT a loop, and not a long body.** Disassembled with
`external/psxport/tools/disasm.py` over the header-driven RAM image (`tools/ram_image.py`,
595,968 B placed at `[0x80010000,0x800A1800)`) plus `FMV/FMV.BIN` injected at its proven slot
`0x800D5D20`. `0x80094158 lhu $v0,($a2)` is the tail of a **queue-pop function entered at
`0x800940F4`**, which reads the head at `0x800C9504` (`0x800940FC lw $v0,-0x6afc($v0)`, with
`0x800940F8 lui $v0,0x800d`, so the target is `0x800D0000 - 0x6AFC`), the
queue base pointer at `0x800CE1B0` (`0x80094104 lw $v1,-0x1e50($v1)`), scales the head by 32
(`0x80094150 sll $v0,$v0,5`), and returns the entry's state word. Word 1 means "in progress",
word 2 means "complete" (`0x8009411C bne $v0,$v1(=1),0x80094158` and
`0x80094164 bne $v0,$v1(=2),0x800941B0` = return). The spin itself is the proof that the pop
keeps answering "no event", because `0x800D6DEC beqz $v0` only re-enters the loop while the pop
returned 0.

**THE QUEUE HEAD IS `0x800C9504`, NOT `0x800D9504`, AND THAT CORRECTION RETRACTS A CLAIM IN THIS
FILE.** `0x800940F8 lui $v0,0x800d` then `0x800940FC lw $v0,-0x6afc($v0)` reads
`0x800D0000 - 0x6AFC = 0x800C9504`; an earlier revision of this file wrote `0x800D9504`, and the
watchpoint that "proved" the module payload clobbered the head had been placed on
`$core->ram + 0xD9504` — the wrong word, `0x800D9504`, which really *is* inside the module image
and really does hold the file's bytes (`b6 ff 01 04` at file offset `0x37E4`, which Capstone
decodes as `bgez $zero,0x800d93e0`, i.e. branch-to-next-word alignment padding). That is why it
read `0xB6` consistently: it was a code word of `FMV.BIN` being copied correctly. **The "the
module payload overwrites the stream's live queue head" claim is withdrawn — it was a wrong
address read twice.** The base pointer `0x800CE1B0` (`0x800D0000 - 0x1E50`) was right, and its
single write to `0x801421E0` in a whole run is real, so the stream's base **is** initialised once.

What the corrected addresses leave standing, all measured:

* The pop is the queue-pop of a 32-byte-stride ring: head at `0x800C9504`, base pointer at
  `0x800CE1B0`, entry state halfword at offset 0, `0x80094110 lhu $v0,($a2)`, state 1 = in
  progress, 2 = complete, and the state-1 path clears the entry and re-reads
  (`0x8009413C sh $zero,($a2)`) and clears the head (`0x80094130 sw $zero,-0x6afc($at)`).
* **`[0x800C9504]` has ZERO writers in a whole run** and reads `0x00000000` at both ends. The pop
  is therefore always polling **entry 0** of the queue, and no guest code advances the head — which
  is consistent with a single-entry, in-order stream rather than a ring the guest walks.
* `[0x800CE1B0]` is written **exactly once**, to `0x801421E0`, from translated guest code. **But it
  reads `0x000000E0` at the abort in some runs and `0x801421E0` in others**, for the same PC and
  the same cycle count, and the second change is *not* attributed to any write the watchpoint
  reports. That is an open diagnostic, not a guest fact: either a store from the framework's CD
  worker thread (the watchpoint is a per-core debug register) or a second alias. Naming it as
  "the base pointer is 0xE0" would repeat exactly the mistake this section is retracting.
* The stream is live at the abort: `cd stream_active=1 setloc_lba=12718`, `I_STAT=0x00000004`
  (CD data-ready) with `I_MASK=0x0000000d` (enabled), and the guest's own STR ready callback
  `0x80093D84` is installed in `[0x800A0808]`.

**WHO WAITS, AND WHY IT IS FINITE.** `ra=0x800D6DEC` is the instruction after
`jal 0x800940F4` inside the FMV overlay's per-frame fetch at `0x800D6DC8`, whose loop is
**bounded**: `0x800D6DD8 lui $s0,0x80` (0x8000 = 32,768 attempts), `0x800D6DEC beqz $v0,…`
(spin while empty), `0x800D6DF4 bnez $s0,0x800D6DE4`, and on exhaustion
`0x800D6DFC j 0x800d6f58` returning 0. 32,768 empty pops at ~17 cycles each is ≈557k cycles —
**the whole one-turn budget, spent on the give-up path.** So the body is finite and small: this
is NOT "a legitimately long finite body" and NOT a divergence. It is a wait for an event that
never arrives, and the guest's own bounded give-up is what exhausts the budget.

**THE EVENT IS NEVER DELIVERED, AND TWO OWNERS ARE MEASURABLY DEAD IN THIS RUN.**

1. **DMA completions reach the framework's dispatcher and are then thrown away.**
   `[dmairq] owed ch4 -> callback 00000000 (guest slot 00000000)` — 1 occurrence in the run.
   `GameConfig::dmaCallbackTable` is `0` (`game/core/game_config.cpp:321`), so
   `dma_callback_slot(0, ch)` is 0, the arm reads no callback, and the completion is consumed
   (`hle_interrupt.cpp:121`: "nothing registered: the completion is consumed"). The guest's
   own per-channel DMA completions — the ones the STR path's CD/MDEC chain depends on — can
   never run.
2. **The CD data-ready interrupt is raised, enabled, and never served.**
   `[irq] CD raised IRQ2 -> I_STAT=0x004 (mask=0x00D, ENABLED)`, 1 occurrence, in the same
   millisecond as the stop, with **0 `cdirq` lines in the whole run**. The framework's own
   CD-ROM interrupt handler is gated off for this title: `cdReadyCallbackOwnedByGuestInterrupt()`
   returns false as soon as `core.cfg` is non-null (`cd_ready_delivery.cpp:78-84`), and Toy
   Story 2 is a `LegacyGameRuntimeAdapter` runtime, so `core.cfg` is always set. The
   `HostPump` alternative (`Cd::pumpStream`) is called from **no product code path** — only
   from `tests/test_cd_stream_callback.cpp` and `tests/test_cd_ready_delivery.cpp`. So for this
   title the CD completion has no owner at all.

**Guest progress immediately before the stop is real, not a replay**: FMV generation 4 published
(`authenticated FMV/FMV.BIN image 4:4 at 0x800D5D20, 510960 bytes`), `CdRead 1 sector from LBA
16`, `CdRead 1 from LBA 18`, `CdRead 4 from LBA 12717`, `DMA0 complete` ×2,
`w DICR0[4] = 00920000 -> 00920000 (armed channel mask 12) ra=800D7130`, `display depth ->
24-BIT (GP1(08)=08000010, 256x240)`, then the give-up spin.

**AND THE STREAM'S STATE IS SET UP BUT NEVER ADVANCES.** A hardware watchpoint over a
whole run, taken from a live `core->ram` pointer at a frame where that symbol exists, is the
measurement (a watchpoint on the guest address `0x800A0808` itself cannot be inserted — the guest
RAM is a host buffer, and asking gdb for that address fails with "Cannot access memory"):

* `[0x800CE1B0]` (the queue base pointer) is written **exactly once** in the run, to `0x801421E0`.
  The queue base **is** initialised once, by the stream.
* `[0x800A0808]` (the CD ready-callback slot) is written **twice**, and the last write is
  `0x80093D84` — the guest installing **its own** STR ready callback through its own setter
  `0x80090D28` from the call site `0x80093D50 lui $a0,0x8009 / 0x80093D54 addiu $a0,$a0,0x3d84`.

So the stream *does* start: base pointer written once, ready callback registered, and at the
abort the framework still reports `cd stream_active=1 setloc_lba=12718` with the CD data-ready
interrupt pending and enabled. What it never does is post a completion, and the head that selects
the post slot is never written at all (`0x800C9504`: zero writers, `0` throughout) — so the guest
is polling entry 0 and waiting for something that should have been posted there.

### RETRACTION — the `0x00000084` in the section above was MY OWN BAD READ, not a guest fact

An earlier debugger capture printed `[0x800A0808]=0x00000084` at the abort and this issue used it
to warn that `cdReadyCbPtr` might be the wrong address. **That is withdrawn.** The capture read
`$core->ram[...]` from frame 2 of the `abort()` backtrace, which is not the `callGuestToReturn`
frame on that path (`No symbol "core" in current context`), so it read an adjacent word of an
unrelated object. With the watchpoint placed from a frame where `core` is real, the slot holds
`0x80093D84` — a valid guest code address, installed by the guest. **`GameConfig::cdReadyCbPtr =
0x800A0808` (`game/cd/stock_libcd_layout.h:30`, bound at `game/core/game_config.cpp:316`) is
CONFIRMED correct**, by three independent things: the static store census, the decoded setter
`0x80090D28 lw $v0,0x808($v0) / sw $a0,0x808($at) / jr $ra` with 11 direct `jal` call sites, and the
live watchpoint. The lesson is the one this workspace has already paid for twice: a value read at
the wrong frame is a measurement, and it is not a measurement of the thing it names.

**One more framework-owned observation, measured in the same run.** The sibling slot
`[0x800A0804]` is written **50 times**, every write from `cd_command_stock_sync`
(`psxport/runtime/psx/cd_override.cpp:254`) or `cd_sync_stock_sync` (same file `:273`), alternating
`0x80090B0C` and `0x00000000`. The framework's stock-libcd owner is deliberately installing and
removing a guest-visible CD callback on every CdSync. That is the framework reproducing a measured
library behaviour, it is a *different* slot from the ready callback, and it is not the defect —
but it is the reason a "watch the CD callback slot" probe sees dozens of writes and must name
which slot it means.

**WHAT THIS CHANGES AND WHAT IT DOES NOT.** It removes the "long finite body" and "divergence"
readings of this stop with byte evidence and leaves one: the guest is blocked on a CD/DMA
completion this port never delivers. It also **withdraws an earlier claim in this same file** — that
the queue head was "definitively corrupt" — and replaces it with the measured writer list above.
It does **not** make the movie loop finite by itself — issue 0027's own finding stands
(0x800D7088 plays every STR frame and calls VSync once per frame inside one guest call, which no
one-turn budget can hold), so the loop still has to become a title-owned finite owner. **No fix is
landed and no frame completed: S002 and S003 stay `partial`.**

**NEXT STEP, NAMED.** Three items, in two owners:

* *Title side, one measurement first*: **the queue entry the guest is actually polling.** The head
  is 0 and never written, so the wait is on `lhu` at `base + 0`, where `base` is the value of
  `[0x800CE1B0]` — and that value is not settled (see the unattributed-change bullet). Resolve the
  base first (watch it from a frame where `core` is real, with the CD worker stopped or a
  per-thread watchpoint), then read the entry's state halfword and its remaining 30 bytes. That
  single read names what the guest expects the port to have posted.
* *Title side, `game/cd/stock_libcd_layout.h`*: measure Toy Story 2's per-channel DMA callback
  table and bind `.dmaCallbackTable` (`game/core/game_config.cpp:321`, currently 0) to it, so the
  completions the framework already dispatches (`dma_irq.h:112 dma_callback_slot`,
  `hle_interrupt.cpp:94`) reach the guest's callbacks. The address is still to be measured; a wrong
  address is worse than the honest zero because it would dispatch a garbage pointer.
* *Framework side, `runtime/psx/cd_ready_delivery.cpp:78-84`*: the `if (core.cfg) return false;`
  gate means no `GameConfig` title can ever use the framework's own CD-ROM interrupt handler,
  and nothing in the product calls `Cd::pumpStream`. Either the gate must consult a declared
  layout even when `cfg` is set (psxport issue 0124 already records that half of the gate as
  "defence-in-depth no test can currently reach"), or the product's host turn must pump the
  stream. This port cannot make that call from its own tree.
* *Do not "fix" the CD ready slot*: it is already right. What the watchpoint did find is that the
  guest's STR ready callback `0x80093D84` is installed and never invoked, because the framework's
  CD data-ready arm is gated off for this title (item 3).
