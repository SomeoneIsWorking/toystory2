# Project state

Factual capability coverage for the Toy Story 2 port. Epic intent lives in
`docs/project-goals.md`, atomic work in `docs/issues/`, ownership in `docs/codemap.md`, and binary
evidence in `docs/re-frontier.md`.

## Comparison baseline

The baseline is the unmodified USA PlayStation release `SLUS_008.93` running in a general-purpose
emulator. Intended differences are a standalone native/dynarec product, host-owned finite frame
iteration, native input and render ownership, true widescreen, and interpolated presentation.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | USA executable, disc files, and loaded modules are reproducibly identified and placed | verified | — | G001 |
| S002 | psxport executes resident and streamed code through a gameplay dynarec with classified bounded fallback | partial | S001 | G001 |
| S003 | Native title owners provide finite frame, timing, input, audio, and presentation sequencing | partial | S002 | G001 |
| S004 | The current product boots through front end and LEVEL01 gameplay | blocked | S002, S003 | G001 |
| S005 | Host input produces repeatable guest gameplay behavior | partial | S003, S004 | G001 |
| S006 | Guest-rendered 15-bit screens and gameplay present coherently | blocked | S002, S003 | G001 |
| S007 | Toy Story 2's 24-bit MDEC movies present coherently | partial | S002, S003 | G001 |
| S008 | Authored projection is published to title-owned consumers | partial | S002 | G002, G003 |
| S009 | Visible scene layers have native game-state producers | missing | S008 | G002, G003 |
| S010 | True widescreen composes correct title and gameplay pictures | missing | S008, S009 | G002 |
| S011 | Presentation interpolates stable authored state at a player-facing 60fps cadence | missing | S008, S009 | G003 |
| S012 | Traveller's Tales `.RAW` assets are reproducibly framed and decompressed | verified | S001 | G001 |
| S013 | The fresh-clone launcher builds and starts the intended product | partial | S001, S002 | G001 |
| S014 | Hosted CI truthfully distinguishes repository policy from native product support on Linux, Windows, macOS, and Android | partial | S002 | G001 |

## Current focus

**S002**, in parallel with the active Spyro 1 title; nothing it lands may regress Spyro 1's gates.
Finish list, in order:

1. ~~**Classify the strict guest-call budget exit at `0x80094158`**~~ — **DONE, issue #27**: an
   undelivered event, and the guest's side of the chain is now measured end to end. The FMV's
   bounded pop (`0x800D6DC8`, 0x8000 = 32,768 tries) waits on a 32-byte ring that only
   `0x80093E88` can satisfy, and it writes state `2` **itself, synchronously**, from `0x80094B30`.
   Both guards on that call are measured: `0x80094B14` on `[0x800CE148]`, whose one writer is a
   **clear** and whose value **never changes in a run**, and `0x80094B28` on `[0x800C1170]`, the
   end-of-stream flag, which **never changes either** — so the FMV's last sector is never reached.
   52 instruction words of that chain are asserted by `tools/verify_str_completion.py` (gated, with
   a selftest) and typed as `ts2::cd::kStrCompletionLayout`.
2. **The CD data-ready delivery**, framework-side and assigned to the psxport cd-complete agent:
   `runtime/psx/cd_ready_delivery.cpp:78-84` refuses the framework's own CD-ROM interrupt handler
   for any `GameConfig` runtime, and no product code calls `Cd::pumpStream`. At the abort the sector
   is owed (`cd stream_active=1 setloc_lba=12718`), the data-ready interrupt is raised and enabled
   (`I_STAT=0x004`, `I_MASK=0x00D`), and 0 deliveries are made. **This is the one open item.**
3. **First verified frame**: one completed presentation fence with a whole-run translated/fallback
   ledger (S002, S003).
4. **Front end**: FMV and MEMORY loop ownership (issues 0026, 0027) through to the front-end menu on
   Lightrec, then Andy's Room (S004).
5. Then player control (S005), presentation coherence (S006, S007), and only after gameplay runs,
   widescreen (S010) and 60 fps (S011).

**Measured and closed on the title side, not worked around:** `.dmaCallbackTable` stays `0` because
there is no per-channel DMA callback table for this title to bind. psxport's `dma_irq.h:112`
documents it as the table the **BIOS** keeps and the guest fills through the SDK's `DMACallback`, a
B0-vector BIOS entry — the guest reaches the BIOS only as `jr 0xB0` with the function number in the
delay slot (**24** such gate sites counted) — and this port has no BIOS ROM. A census of runtime RAM
over the two regions such a table could occupy matched **3** runs in `0x800A0000`–`0x800D0000` and
**0** in `0x801F0000`–`0x80200000`, and the tool **classifies** every entry rather than filtering:
the largest run (160 bytes at `0x800A082C`) is 28 `ascii` — the Sony library's own symbol strings —
and the only real entries are 3 of 5 at `0x800A0CB4`, not a 4- or 7-entry channel table. The
reasoning is recorded in `game/core/game_config.cpp` and asserted in both directions by the
boundary test.

## Capability details

### S001 — Reproducible retail inputs and module placement

Evidence: `tools/extract_exe.py` identifies the 598,016-byte executable; exact identity is recorded in
`docs/info/exe-identity.txt`. `tools/overlay_map.py` derives the LEVEL slot at `0x800D12C0`, the shared
MEMORY/FMV slot at `0x800D5D20`, and FMV entry `0x800D6628`. The disc census identifies 22 loaded code
modules without tracking game bytes.

### S002 — Dynarec-first guest execution

The obsolete offline translator, emitted source corpus, seed manifest, generated registry, product
selector, and static-only tests are absent. Title guest calls and native overrides use psxport's typed
runtime APIs.

The Linux x86-64 Clang/Ninja product links maintained Lightrec `b1457137`. The obsolete static
executor is absent, and linked execution-boundary, structure, and title tests exercise the runtime
dispatch path. The title authenticates `BITS/MEMORY.BIN` against the exact disc SHA-256, checks all
63,312 transferred bytes, invalidates the translated range, and publishes a new image generation on
replacement. A real retail run reached MEMORY generations 2 and 3.

The earlier first-frame stop at `0x8002149C` was inside the resident RAW decoder `0x80021190`,
called by front-end asset loading. A debugger snapshot showed the live 160,484-byte source buffer was
byte-identical to the exact LEVEL00 `LEVEL.RAW`; its source cursor had advanced to the first chunk
boundary and its destination cursor was advancing. The file's seven chunks decode 886,376 bytes with
both CRCs and a terminal sentinel. A debugger-only continuation preserved the original return
sentinel and returned through Lightrec with zero fallback. The title now applies the existing 64-slice
finite initialization refusal contract only to that front-end load and the earlier graphics-table
construction; a synthetic non-returning call is refused after two slices. The next bounded retail
run completed graphics initialization in two slices and the cold front-end asset load in **28 slices /
15,245,664 cycles**, returning to `0x8007A9E8`.

That run then entered FMV at `0x800D6628` without an active FMV code-image identity and refused after
46 cycles. The title's shared-slot owner now observes the exact `fmv\\fmv.bin` file load at `0x800D5D20`,
authenticates the 510,960-byte source digest and transferred bytes, retires the replaced MEMORY
identity, invalidates the shared executable slot, and publishes one FMV generation. Synthetic tests
reject a wrong path, destination, source digest, length, and transferred byte and verify
MEMORY→FMV→MEMORY replacement. The retail corpus verifier compares the shipping FMV path, size, and
digest against the executable and file.

An earlier bounded run published authenticated `FMV/FMV.BIN` generation 4, then refused
`CdRead(1 sectors)` and exhausted its strict call budget at `0x800940F4`. Issue #27 identifies
the refused read's cause as zeroed GetTN/GetTD results across the native command and CdSync
boundary. With psxport `9c7dd098`, a new authentic-disc run again published FMV generation 4,
returned the first FMV `CdRead(1)` successfully from LBA 16, reached two further FMV read
entries, and switched display to 24-bit. Its next stop was a strict frame-driver guest-call
budget exhaustion at `0x80094158` after 564,492 cycles.

**That exit is now classified: an undelivered event, not a long body and not a divergence.**
A gdb backtrace at the abort names the call — `0x8003EE4C(2, 0)` (`kMemoryStatus`,
`toystory2_frame_driver.cpp:143`) — and the guest registers at the exit are
`pc=0x80094158, ra=0x800D6DEC`, i.e. the FMV overlay's per-frame queue fetch. Disassembly over
the header-driven RAM image shows `0x80094158` is the tail of the queue-pop function entered at
`0x800940F4` (head `0x800C9504`, base pointer `0x800CE1B0`, 32-byte stride, state words 1 and
2), and its caller at `0x800D6DC8` spins it a **bounded 0x8000 = 32,768 times** before giving
up. Two delivery owners are measurably dead in the same run: `[dmairq] owed ch4 -> callback
00000000 (guest slot 00000000)` (1 of 1) because `.dmaCallbackTable` is 0, and
`[irq] CD raised IRQ2 -> I_STAT=0x004 (mask=0x00D, ENABLED)` (1 of 1) with **0 `cdirq`
deliveries**, because the framework's CD-ROM interrupt arm is gated off for any `GameConfig`
runtime and no product code calls `Cd::pumpStream`.

**The blocked edge has since been measured end to end, and it is the CD read, not the DMA path.**
The guest posts the completion itself, synchronously, from `0x800941D8` via `0x80094B30` to
`0x80093E88`, which writes state `2` into the entry the pop reads. Both guards on that call are
measured: `[0x800CE148]` (`0x80094B14`) never changes value in a run — its one writer is a clear,
and the stream is never opened — and `[0x800C1170]` (`0x80094B28`), the end-of-stream flag, never
changes either, so the FMV's last sector is never reached. At the abort the sector is owed
(`cd stream_active=1 setloc_lba=12718`), the data-ready interrupt is raised and enabled
(`I_STAT=0x004`, `I_MASK=0x00D`), and **0** deliveries are made, because the framework's CD-ROM
interrupt arm is gated off for any `GameConfig` runtime and no product code calls `Cd::pumpStream`.
52 instruction words of the chain are asserted by `tools/verify_str_completion.py` (gated, with a
selftest) and typed as `ts2::cd::kStrCompletionLayout`. No fix is landed and **no frame completed**,
so there is still no whole-run fallback ledger, gameplay, or performance claim. Full evidence is in
issue #27.

### S003 — Native finite frame ownership

The retained title modules own the measured front-end/resident state machine, field quota, native pad
packets, deferred display service, audio step, and one presentation commit without guest VSync. Their
hermetic boundary tests predate the execution migration and the sources now use the typed dynarec
guest-call adapter.

Gap: the retained boundary tests pass and FMV CD reads progress, but the runtime stops at a strict
guest-call budget exit before its first completed frame. That exit is classified and its guest-side
chain measured end to end (issue #27: the FMV never reaches its last sector, because the CD
data-ready interrupt is raised, enabled and never served) but **not fixed** — the one open item is
framework-side. MEMORY and FMV loop ownership also remain incomplete under issues #26 and #27.

### S004 — Current boot through gameplay

Blocker: S002. Earlier execution evidence reached coherent Andy's Room, but it used the removed
executor and is only a scenario expectation, not evidence for the current product.

### S005 — Repeatable player control

The exact retail pad buffers and native active-low packet producer remain implemented. Earlier runs
paused, unpaused, and moved the camera, while exact sample replay diverged at a later pause transition.

Gap: the scenario must be reverified through the current dynarec product after S002 and S004.

### S006 — Coherent 15-bit presentation

Blocker: S002. The guest GPU/presentation path and native frame owner remain, but no current title gameplay frame
has been produced through Lightrec.

### S007 — Coherent 24-bit MDEC movies

Gap: the MDEC path reaches 24-bit mode in retained evidence, but the captured frame had duplicated noisy
columns and was mostly black. Issue #22 owns the independent raw-VRAM versus reference discriminator.
The current product must first cross S002 before that fault can be retested.

### S008 — Authored projection publication

Exact executable evidence derives projection leaves `0x80083CD4` and `0x80083CF4` and initialization
values `256/120/160`. The title retains its hermetic publication boundary and runtime native owner.

Gap: current live reach and remaining culling/projection writers need verification through S002.

### S009 — Native scene producers

Missing capability: title code captures authored camera, visibility-list, instance, and mesh-command
inputs, but no title-owned world, actor, effect, or 2D producer emits the visible picture. Issue #30
owns the semantic producer boundary.

### S010 — True widescreen

Missing capability: expanding the guest draw canvas crosses fixed VRAM parity and exposes invalid
columns. Correct widescreen requires a semantic native producer plus projection, viewport/scissor,
culling, and 2D layout ownership. Stretching and frame sampling are excluded.

### S011 — Interpolated 60fps presentation

Missing capability: camera history exists, but there is no identity-matched object history or visible
native consumer. Interpolation remains unavailable until S009.

### S012 — `.RAW` asset decoding

Evidence: `tools/raw_probe.py` verifies framing and packed CRCs; `tools/raw_unpack.py` implements the
Traveller's Tales LZ scheme and verifies both CRCs for 813/813 chunks across 46 files. Independent
LEVEL01 extractions match all 39 decoded chunks.

### S013 — Fresh-clone product launcher

The three-line `run.sh` enters the frozen uv environment. `tools/run.py` resolves psxport, configures
the shared top-level `build/player` tree, builds `build/player/bin/toystory2_port`, and launches only
that target. Its injected-host suite passes the help, dependency refusal, compiler portability,
explicit disc, build-only, and zero-argument paths.

Gap: the default product cannot enter gameplay until S002, and the cold real build/launch path has not
been rerun for this migration.

### S014 — Platform CI coverage

Partial capability: `.github/workflows/ci.yml` runs the asset-free launcher and structure contracts
on one Linux host with full history, read-only permissions, pinned actions, and an explicit timeout.
The workflow resolves the exact framework recorded in `psxport.pin` before those tests; the prior
hosted run (`34223500014`) failed because `external/psxport` had not been created, while a fresh
checkout run of that resolution and the launcher tests now passes locally. It does not present
those Python contracts as a native Linux or macOS build.

| Platform | Applicability | Current CI evidence and exact gap |
| --- | --- | --- |
| Linux x86-64 | applicable desktop target | Local native product build and title boundary tests pass; hosted native runtime and package qualification remain missing. |
| Windows x86-64 | applicable desktop target | Missing: no current Windows native/dynarec build, runtime test, first-run setup, or package boundary exists. |
| macOS arm64 | applicable desktop target | Missing: no current Apple-Silicon native/dynarec build, runtime test, first-run setup, or application package exists. |
| Android arm64 | applicable future portable target | Missing: no Android title integration, shared `android-port` consumer, native runtime, APK build, or install test exists. |

Gap: add each platform job when the matching redistributable runtime/package boundary exists and is
asset-free. The same launcher-policy tests on multiple hosted operating systems would not prove
platform support.
