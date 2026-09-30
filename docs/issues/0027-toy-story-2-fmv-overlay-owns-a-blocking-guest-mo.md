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

**AND THE RING'S OWN STATE IS NOW SETTLED.** A hardware watchpoint over a whole run, placed with
explicit `unsigned int *` casts from a frame where `core` is real, gives:

* `[0x800CE1B0]` (the ring base pointer) is written **exactly once**, to `0x801421E0`, and **never
  changes again** — the high-halfword watchpoint never fires either, so no narrow store hides. That
  retires the "0x801421E0 in one run, 0xE0 in another" discrepancy: there was only ever one value,
  and the `0xE0` was the low **byte** of it (see the retraction below).
* `[0x800CE148]` (guard 1) **never changes value** in a whole run. Its one writer stores `$zero`, so
  a value watchpoint cannot fire on it — which means "no fires" is "no change", not "no writer".
  It is 0 from boot and stays 0.
* `[0x800C1170]` (guard 2) **never changes value** either, so the FMV's end-of-stream setter at
  `0x800949C0` **never runs**: the last sector is never reached.

**Which closes the chain, and points the undelivered event at the CD read rather than at the DMA or
the guard.** The ring is initialised (base pointer written, head reset), the pop then waits for the
end-of-stream post, and the post requires the stream to have finished its last sector — which
requires the CD data-ready delivery that this port's framework currently does not perform for a
`GameConfig` runtime. At the abort the framework reports `cd stream_active=1 setloc_lba=12718` with
`I_STAT=0x00000004` (CD data-ready) and `I_MASK=0x0000000d` (enabled): the sector was owed and the
interrupt was raised and enabled, and nothing served it. That is the same conclusion the earlier
`[irq] CD raised IRQ2 ... 0 cdirq deliveries` measurement reached, now with the guest's own chain
measured end to end behind it.

### RETRACTION — the `0x00000084` in the section above was MY OWN BAD READ, not a guest fact

An earlier debugger capture printed `[0x800A0808]=0x00000084` at the abort and this issue used it
to warn that `cdReadyCbPtr` might be the wrong address. **That is withdrawn, and the cause is now
known exactly.** `Core::ram` is `uint8_t ram[0x200000]` (`runtime/psx/core.h:36`), so in gdb
`$core->ram[0xA0808]` yields **one byte**, and the script printed that byte with `%08x`. `0x84` is
the low byte of `0x80093D84` — the value a hardware watchpoint placed with an explicit
`*(unsigned int *)($core->ram + 0xA0808)` reports. The same bug produced the other phantom value
below: `0xE0` is the low byte of `0x801421E0`.

**`GameConfig::cdReadyCbPtr = 0x800A0808` (`game/cd/stock_libcd_layout.h:30`, bound at
`game/core/game_config.cpp:316`) is CONFIRMED correct**, by three independent things: the static
store census, the decoded setter `0x80090D28 lw $v0,0x808($v0) / sw $a0,0x808($at) / jr $ra` with
11 direct `jal` call sites, and the live watchpoint. The lesson is the one this workspace has
already paid for twice: a value read at the wrong frame is a measurement, and it is not a
measurement of the thing it names. Here it was worse — a value read at the wrong **width**, from a
perfectly good frame, which is a variant nothing in the existing notes warned about.

### MEASURED 2026-09-30 — THE STR COMPLETION CHAIN, and the DMA callback table that does not exist

**The blocked edge is now located to a single `beqz`, from the retail bytes.**

The FMV's bounded pop at `0x800D6DC8` reads a 32-byte-stride ring, and exactly one routine in the
reachable images can satisfy it. All 52 instruction words below are asserted by
`tools/verify_str_completion.py`, which refuses on any change and is gated by its own selftest.

| step | instruction | what it does |
|---|---|---|
| `0x800D72B4`/`0x800D72B8` | `lui $a0,0x8015` / `lw $a0,0x2908($a0)` | the FMV reads its ring descriptor at `[0x80152908]` |
| `0x800D72BC` | `jal 0x800D7F58`, `$a1 = 0x4000` | the module allocator produces the ring |
| `0x800D72CC` | `jal 0x800907FC` | the ring init, with the allocation and entry count 1 |
| `0x80090808` | `sw $a0,-0x1e50($at)` | **the image's only writer of `[0x800CE1B0]`** |
| `0x80093DB8` | `sw $zero,-0x6afc($at)` | the init clears the head `[0x800C9504]` |
| `0x80094104` | `lw $v1,-0x1e50($v1)` | the pop reads the same base pointer |
| `0x80094108` | `0x00021140` `sll $v0,$v0,5` | stride 32 |
| `0x80094110` | `lhu $v0,($a2)` | the pop reads the entry's state halfword |
| `0x80093E94` | `lw $v1,-0x1e50($v1)` | the post reads **the same** base pointer |
| `0x80093EA0` | `0x00021140` | the post scales by the **same stride word** |
| `0x80093EA8`/`0x80093EAC` | `addiu $v0,$zero,2` / `sh $v0,($v1)` | **the post writes 2 — "complete" — into that halfword** |
| `0x80094B04`/`0x80094B14` | `lw $v1,-0x1eb8($v1)` / `beqz $v1,0x80094b38` | **guard 1: `[0x800CE148] == 0` skips the post** |
| `0x80094B20`/`0x80094B28` | `lw $v0,0x1170($v0)` / `beqz $v0,0x80094b38` | **guard 2: `[0x800C1170] == 0` skips the post** |
| `0x80094B30` | `jal 0x80093E88` | the only call to the post |

So the completion is not missing because of a CD or DMA delivery path: the guest writes it
**itself, synchronously**, from `0x800941D8`'s channel setup. The two guards are why it never does.

**Guard 1 has no setter.** A `lui`-formed-store census over all three images (292,560 words) finds
exactly **one** writer of `[0x800CE148]` — `0x80093FA8 sw $zero,-0x1eb8($at)`, the stream open, which
**clears** it. The census states its own blind spot (a store through a register loaded from a pointer
is not counted) and the tool **refuses** if it ever returns zero for a word it knows is written, so
a zero here cannot be a broken read. What it cannot yet exclude is a store through a loaded base, or
a writer in code that does not exist in any of the three images.

**Guard 2 is the end-of-stream flag, and that is by design.** Its only non-zero store is
`0x800949C0 sw $v1,0x1170($at)`, reached only when the sector counter runs out
(`0x800949A0`–`0x800949AC`). It is zero mid-stream and 1 on the last sector, so the run dying with
`[0x800C1170] = 0` is *correct* guest state, not a lost write. The real question is guard 1.

**THE PER-CHANNEL DMA CALLBACK TABLE: measured to not exist for this title.** psxport's
`dma_irq.h:112` documents the contract: the table is the one the **BIOS** keeps, which the guest
fills through the SDK's `DMACallback(ch, fn)` — a BIOS B0-vector entry. The guest reaches the BIOS
only as `addiu $t2,$zero,0xB0` / `jr $t2` with the function number in `$t1` in the delay slot
(`0x800893B4`, `0x800893B8`, `0x800893BC`; **24** such gate sites counted in the executable), so
filling that table needs the BIOS ROM's DMA interrupt handler, which this port does not have.

Two independent measurements back that, and one of them caught a false positive worth recording.
Both are now tool output (`--ram-bss` / `--ram-top` on a runtime dump), not prose:

* The census of the two RAM regions such a table could occupy (`0x800A0000`–`0x800D0000`,
  196,608 bytes, and `0x801F0000`–`0x80200000`, 65,536 bytes) matches **3 runs** in the first and
  **0 runs** in the second. Entries are then **classified**, not filtered, because a filter cannot
  separate the two kinds of thing that live in the text's address range:
  * `0x800A082C`, 160 bytes: **28 `ascii`, 12 `other`** — this is the Sony library's own symbol
    **strings** (`"CdSync"`, `"CdReady"`, `"CdGetSector"`, `"CdStSync"`, `"CdNolop"`, `"DiskError"`,
    `"DataEnd"`, `"AckNol"`, at `0x80023608`+). They pass any "is it a code pointer" range test, and
    the tool says so in its own output.
  * `0x800A0AF0`, 16 bytes: 3 `other`, 1 `ascii` — mixed, so not a table.
  * `0x800A0CB4`, 20 bytes: **3 `function`, 2 `other`** — the only real entries, and two of the five
    words are mid-function addresses. Not a 4-entry or 7-entry channel table.
  * **So: no run of real function entries of a DMA-channel-table's shape exists in either region.**
* The title does not need one: its completion is the synchronous chain above.

The census's own controls are in its selftest, because a zero is worthless without them: it plants
an ASCII run and requires the `ascii` classification, plants a mixed run and requires exactly
`['function', 'other', 'other']`, and plants a store and requires the census to find it. The static
census additionally **refuses** if it ever returns zero for a word it knows is written — which is
what caught two of this tool's own defects during this work (an unsigned displacement comparison
and a missing `sw` opcode, either of which made it report a clean zero).

**Therefore `.dmaCallbackTable` stays 0, with the reasoning recorded in `game/core/game_config.cpp`
rather than left as a bare zero.** Binding an unmeasured address would dispatch a garbage pointer,
which is strictly worse than the framework's documented "no dispatch at all". The boundary test
asserts both halves — that the declared base is 0 and yields no slot for any of the 7 channels, and
that a non-zero base addresses the per-channel stride — so a future change cannot slip in as an
invisible improvement.

**AND THE DISPUTED `[0x800CE1B0]` VALUE IS RESOLVED.** Measured by watchpoint: written exactly
once, to `0x801421E0`, never changed again. There was never a second value — see the retraction
below.

### MEASURED 2026-09-30 — the guard words never change, so the missing edge is the CD READ

The decisive run watched `[0x800CE1B0]` (word and high halfword), `[0x800CE148]` and
`[0x800C1170]` with explicit `unsigned int *` casts, from a frame where `core` is real.

* `[0x800CE1B0]`: **one write, `0x801421E0`, no change afterwards.** The halfword watchpoint never
  fires, so no narrow store is hiding. The earlier "0xE0" was the low **byte** of this same value.
* `[0x800CE148]`: **never changes value.** Its only writer stores `$zero`, so a value watchpoint
  cannot fire — "no fires" means "no change", not "no writer". It is 0 from boot and stays 0.
* `[0x800C1170]`: **never changes value**, so the FMV's end-of-stream setter at `0x800949C0` **never
  runs**: the last sector is never reached.

That last point is what re-aims the whole diagnosis. The FMV's pop waits for the **end-of-stream**
post, the post requires the last sector, and the last sector requires the CD data-ready delivery
this port's framework does not perform for a `GameConfig` runtime. At the abort the framework
itself reports `cd stream_active=1 setloc_lba=12718` with `I_STAT=0x00000004` (CD data-ready) and
`I_MASK=0x0000000d` (enabled): the sector was owed, the interrupt was raised and enabled, and
nothing served it. Guard 1 is 0 because the stream was never opened, not because a registration
was lost — so **"find guard 1's setter" is the wrong next step**, and this section supersedes it.

**One framework-owned observation, measured in the same run.** The sibling slot
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

**NEXT STEP, NAMED.** Two items, in two checkouts, and the title-side one is done:

* *Title side — DONE and gated.* `.dmaCallbackTable` is measured to have no table to bind; the
  reasoning is recorded in `game/core/game_config.cpp`, the 52-word chain is asserted by
  `tools/verify_str_completion.py` with its own selftest, the ring facts are typed as
  `ts2::cd::kStrCompletionLayout`, and the boundary test asserts the declared zero in both
  directions. Nothing further here.
* *Framework side, and now the ONLY open item: `runtime/psx/cd_ready_delivery.cpp:78-84`.* The
  `if (core.cfg) return false;` gate means no `GameConfig` title can ever use the framework's own
  CD-ROM interrupt handler, and nothing in the product calls `Cd::pumpStream`. The guest-side chain
  behind it is now measured end to end: the FMV waits for an end-of-stream post, the post needs the
  last sector, and the last sector is owed with the data-ready interrupt raised and enabled and
  nothing serving it. Assigned to the psxport cd-complete agent; this port cannot make that call
  from its own tree.
* *Explicitly NOT the next step, and recorded so nobody re-derives it:* looking for a static setter
  of `[0x800CE148]`. The stream is never opened in this run, so guard 1's zero is correct state,
  not a lost registration. If a later run opens the stream, that question becomes live again.
