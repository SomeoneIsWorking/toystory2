# Toy Story 2 — working rules for THIS repo

A PC-native port of **Disney/Pixar Toy Story 2: Buzz Lightyear to the Rescue! (PS1, USA,
`SLUS_008.93` / SLUS-00893)**, Traveller's Tales, built on the
[psxport](https://github.com/SomeoneIsWorking/psxport) static-recompilation framework
(`external/psxport`). psxport recompiles the game's MIPS code to C and supplies the PSX platform layer;
this repo supplies the game — the seam, the RE, and the native reimplementations.

**The framework rules are NOT restated here. Read `external/psxport/CLAUDE.md`** — it is the authority
for how a game consumes psxport: the CVar ladder, the seam, `generated/` being sacrosanct, RE-first,
diagnostics through `lucent`, the registries, and never editing `external/psxport`. The workspace map is
`external/psxport/docs/workspace/WORKSPACE.md`; the multi-agent protocol is `…/PROTOCOL.md`; the
methodology is `…/docs/porting-a-new-psx-game.md`.

## THERE IS NO DECOMP OF THIS GAME. Everything is Ghidra from zero

This is the single most important difference between this tree and its siblings, and no doc here may
imply otherwise. `vagrant` vendors CC0 `rood-reverse` (~62% matched, 2,299 symbols, byte-identical
targets); `megamanx4` vendors AGPL `sozud/mmx4`, whose declared build target has the same SHA-1 as our
extraction, so **its symbols are literally our addresses**. Both trees therefore have a rule aimed at a
temptation: "a borrowed address is a hypothesis until measured."

**Toy Story 2 has neither.** No decomp exists on decomp.dev or anywhere a GitHub + decomp.dev search
reached — no symbol map, no function boundaries, no matching build for `SLUS_008.93`. There is no
`external/<decomp>` submodule in this tree because there is nothing to put in one. Every guest address
this port ever holds must come from reproducible binary evidence on this executable
(`external/psxport/tools/decomp.sh` — RE-first: never hand-walk disassembly a decompiler will do, and
never black-box debug).

What DOES exist is unusually good third-party **format** documentation for the Traveller's Tales PSX
engine, and one MIT decomp of a *different* TT title. That is a head start on the ASSET PIPELINE and
none at all on the code. `docs/references.md` has the licences, the measured cross-confirmations, and
the traps — read it before touching an asset format, and note that **four of the six relevant projects
carry NO licence at all** (i.e. all rights reserved: read-only).

## Toy Story 2 is STANDALONE — its own repo, no shared `game/`, no lineage scaffolding

Recorded in `external/psxport/docs/workspace/WORKSPACE.md` and re-checked when the similarity metric was
recalibrated. TS2 is cross-studio with **every other binary in the corpus** (18 others as of 2026-08-12;
the corpus held 12 others when the decision was recorded in `WORKSPACE.md`), so **every similarity cell is
a member of the null distribution by construction**. Re-run over the current 19-binary corpus: its largest
is 8.2% with Tomba! 1 = **0.7× the same-PSY-Q null maximum** (11.89%), i.e. below what two unrelated
studios score; then CRASH2 6.2% and SPYRO1 5.8%. All three of those cells have an SDK explanation and no
engine explanation: Tomba! 1 shares TS2's `sys.c` 1.129, and CRASH2 and SPYRO1 are the only two binaries
in the corpus sharing TS2's entire PSY-Q triple (`sys` 1.129 / `intr` 1.76 / `bios` 1.86 — measured here,
`docs/info/claims/004-*`).

So: **one repo, one title.** Do not add `titles/`, do not add a shared `game/`, do not create an
engine-family layer. If a second Traveller's Tales title is ever ported, measure first
(`external/psxport/tools/exe_similarity.py` + `tools/lineage_probe.py` against the recorded null maxima)
— the lineage claim that all 7 TT PSX games share one engine rests on one person's README and **could
not be measured**, because no other TT disc is available on this machine.

## THE STATE OF THIS PORT: headless boot renders the stable retail title

RE-00 supplies Ghidra; RE-01 proves crt0 and RE-03 proves the LEVEL/MEMORY/FMV slots. RE-02 now emits the
identity-checked executable and all 22 proven modules, links the real port, and matches the independent
CPU oracle 34/34 at first call `0x80089344`. The first live miss proved IRQ resume `0x80088A2C`; it is
wired solely as a mid-function re-entry. RE-04 proves the stock-libcd command/result path and deterministic
CDC pacing. The next blocker was not geometry: the guest registered VBlank callback `0x80039D60`, but
the game seam never delivered a host field, leaving wait `0x8003FA68` permanently dependent on a
counter that could not advance. RE-10's now-retired game-local field clock armed after exact graphics
init `0x8003A650`, invoked the registered callback at the GPU field rate, advanced pad/audio and presented.
The same retail headless route changed from zero presents and a 30-second watchdog to non-black legal
and ESRB frames at presents 30, 120 and 900. It then loads `FMV/FMV.BIN` at `0x800D5D20`, enters
`0x800D6628` (file+`0x908`), completes shared BIOS `A0:0x25` path parsing and traverses the resident
renderer's exact 32-way internal jump table. The next defect was title-owned boundary wiring, not
queue capacity: `field_turn` called raw `gpu_present`, so thousands of fields accumulated in one
captured queue. That historical path then committed once per measured field through the neutral shared presentation fence,
without opting into temporal decoration. Real A/B records 8,320 commits, maximum captured field
3,096 under unchanged cap 65,536, and a stable retail title at presents 1,500 and 2,100. RE-14 then
classified LEVEL01 target `0x800D12C4` as a TRUE overlay
entry (module-ID header word at +0, entry prologue at +4, byte-identical residency in the miss RAM
dump, a direct guarded `jal` in boot text plus five descriptor copies storing it) and seeded it; the
next route terminated at unmapped read16 @ 0xEDA4F893 via func_800426E0. RE-16 traces that retired
fault to model-table reset 0x80041F38: jal-shaped mixed FMV data falsely seeded its branch delay slot
0x80041FFC as a function, removing the loop's pointer increment. The generic partition fix restores
the 128-entry clear; a real writer watch observes slot 9 clear/reload and the route continues through
field 10,303 with no fatal or recompilation miss. RE-08 separately closed
the .RAW container: payloads decode with TT's own DecompressRAW LZ scheme (NOT RNC; C019/I015),
813/813 corpus chunks verify both CRCs. RE-06 now closes input delivery: retail pad init passes
`0x800CF8A0`/`0x800CF8C8`, stores them in driver contexts at `0x800A3E98`/`0x800A3F88` with a `0xF0`
stride, and the guest decoder independently reads slot 0. The host field turn writes active-low Cross
`0xBF` and release `0xFF` to that exact packet byte. A bounded real product run now reaches Andy's
Room, opens and leaves the pause menu, and visibly moves the camera under held Right. RE-17 remains
in progress because replaying the exact 14,276 recorded pad samples reaches gameplay but stays paused
at the later captures; issue #20 owns that determinism defect. The per-frame OT/packet-pool layout
is now partial: retail buffer objects prove packet pools `0x801BBFEC`/`0x801DD4E0`, stride `0x214F4`,
and current pointer `0x800A10BC`. A live exact-frame query records 15,521 attribution spans and names
the dominant pre-GTE submitters, but OT extent and title-native producer structures remain unmeasured.
RE-07 is partial rather than empty: retail graphics init `0x8003A650`
calls SetGeomOffset `0x80083CD4` and SetGeomScreen `0x80083CF4` with `256/120/160`; the narrow title HLE
binding preserves their guest effects and records the authored projection on the same Core. Static and
hermetic differential gates prove that boundary. An exact-`dbdb2baf` product run reaches both leaves
four times, first at field 1 with `256/120/160`, and reports zero ABI violations.

RE-18 begins the ownership correction required by the current shared shell. `ToyStory2Runtime` now
creates a title-local FrameDriver and returns from the measured initialization prefix of guest main
`0x8007A9E8` instead of dispatching its non-returning outer loop. The finite owner completes MEMORY
initialization in its first frame, advances one pre-resident poll or interactive-selection iteration,
prepares resident state, and routes normal `0x8007B254` or alternate `0x8007B850`. Its encompassing
frame owns input, two display fields, direct deferred display service `0x80021028`, audio, and one
neutral `commit(...,2)`. A title runtime override preserves graphics init `0x8003A218`'s measured buffer
effects without its two VSync calls and without editing the generated super. Guest VBlank `0x80039D60`
and host turns are absent; linked libetc VSync `0x80088628` is fatal in exact window
`[0x80088628,0x80088770)`. This is not yet an end-to-end product: issue #25 owns independent
MEMORY/FMV loop completion. Bounded real-disc re21 is the current loop witness: it exits itself at the
300-frame cap, reconciles 300/300 frames with zero dropped layers, reaches coherent Andy's Room demo
gameplay, and hits no guest VSync, fatal, miss, unmapped access, or watchdog. This proves the resident
route, not the still-independent MEMORY/FMV owners or indefinite interactive play. A later current-
binary Clang witness, re35, independently exits at 120/120 reconciled frames with zero dropped layers;
its inspected presents 45, 60, 90, and 120 again show coherent Andy's Room gameplay with no guest
VSync or fatal.

The first controlled `aspect=1` run uses that same 300-frame resident route and falsifies the proposed
guest-wide source path: widening the authored 512-pixel draw canvas to 684 crosses each fixed horizontal
double-buffer parity inside 1024-pixel VRAM and exposes wrapped/atlas columns as vertical black slabs.
Issue #24 owns the captures and root cause. True widescreen, Native and lerp now share the correct
pre-GTE frontier under issue #30: resident scene root `0x8002A070`, its object/mesh owners, and their
authored inputs. The first camera boundary is now located at `0x800C1540` (position `+0/+4/+8`, angle
halfwords `+12/+14/+16`) through producer `0x8002C848`; scene root then builds visible lists at
`0x800BB4D8` and `0x800C0AB0`. `ResidentCameraHistory` now captures stable previous/current position
and wrap-aware rotations after each resident update. Bounded real-disc re37 traces changing authored
camera values through 120/120 reconciled frames while inspected Andy's Room captures remain coherent
and no guest VSync/fatal occurs. Dual-method xrefs then separate the stack-local submitted counts for
the two visibility lists from their post-load residency globals. The common owner `0x8002622C`
reaches typed visibility/object/instance records and dominant mesh leaf `0x800100E4`.
`ResidentSceneHistory` now observes both exact entries, captures previous/current candidate batches
and actual mesh-call arguments, and always super-calls the unchanged generated bodies. This builds and
passes its focused boundary, but still awaits the serialized real-disc witness. It is grounded producer
input, not a native picture: vertex/primitive stream, material, and texture semantics remain open.
Post-GTE packet or OT replay is not an acceptable native producer.

`game/core/toystory2_runtime.*` is the title's framework-facing behavior owner. It derives
`LegacyGameRuntimeAdapter` only because generic psxport algorithms still consume the measured
`GameConfig` facts and bounded neutral/fail-fast `GameHooks` callbacks. Guest-main prefix ownership and
FrameDriver construction are runtime overrides. Current psxport `dbdb2baf` retains the guest-VRAM
picture-ownership policy introduced at `bc8c8897`: this title remains legacy-backed, so the adapter projects its verified
immutable answer (`preserveVramBackdrop = 1`) while the current route is wholly guest-rendered. Do not
duplicate that answer in `ToyStory2Runtime`; replace the adapter projection with a derived dynamic
policy only when a measured native producer creates a second ownership state. Remove each legacy
field or callback when a narrow typed runtime interface replaces its last framework reader.

What DOES build today, and is the gate for a change to the seam:

```sh
uv run --frozen python tools/psxport_sync.py --auto                     # resolve external/psxport (symlink to the shared clone, or a private clone at psxport.pin)
uv run --frozen python tools/recomp_substrate.py --ensure
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DPython3_EXECUTABLE="$(uv run --frozen python -c 'import sys; print(sys.executable)')"
cmake --build build --target toystory2_port toystory2_recomp_boundary -j$(nproc)
ctest --test-dir build --output-on-failure                              # launcher, RE, oracle, format and tidy gates
```

`run.sh` is the stable launcher interface: a slim exec shim into `uv run --frozen python bootstrap.py`.
`tools/run.py` owns provisioning and build policy. Its zero-argument route provisions the 22-module
corpus, refreshes generated code only when inputs change, builds only `toystory2_port`, and launches
the current windowed product from the isolated `scratch/build/player` tree with `BUILD_TESTING=OFF`;
it never runs CTest or perturbs the maintainer `build/` cache. `-h`/`--help` print usage and exit 0
before dependency, framework, executable, or disc discovery. The product executable handles the same
flags before installing the title runtime, constructing a `Game`, or attempting disc discovery.
`--prepare-only` exercises the same cold
path and stops before launch. The locked interpreter is propagated into every Python subprocess and CMake.
Explicit `CC`/`CXX` values pass through without compiler-identity checks; when they are absent, CMake
owns compiler discovery. Maintainer verification still uses Clang as shown above.

**`external/psxport` is NOT a submodule any more (2026-08-16)** — it is a symlink to the workspace's
shared framework clone when one exists, or a private clone at this repo's `psxport.pin` on a fresh
machine; `tools/psxport_sync.py --auto` establishes whichever applies. The framework's own nested
vendors (`vendor/lucent`, `vendor/beetle-psx/deps/libchdr`) are the SHARED clone's concern — initialized
once there, never per-port (`--recursive` aborts on beetle-psx's url-less `deps/lightning/gnulib`, and
`sync-submodules.sh` prints "all at recorded gitlinks" over vendors it never reached —
`external/psxport/docs/workspace/KNOWN-DEFECT-sync-submodules.md`). `CMakeLists.txt` fails with a clear
message if they are missing in the shared clone.

## Start here, every task

```sh
uv run --frozen python tools/re_frontier.py next         # which RE step is actually ready to work
uv run --frozen python tools/info.py brief <words>       # what's already proven — and does it still hold?
uv run --frozen python tools/catalog.py search <symptom> # has this been hit (or ruled out) before?
```

Believe these over your instinct about what is known. End the task by writing back what you proved, what
you disproved, and any tool you caught lying. `tools/re_frontier.py` is a SHIM onto the shared engine in
`external/psxport/tools/port/`; do not grow a local copy of it.

## THE DEFINING STRUCTURAL FACT: this game STREAMS CODE OVERLAYS

**Measured 2026-08-12, four independent signals** (`docs/info/claims/002-*`, `003-*`). **21 plain
overlay files** hold
245,148 bytes of R3000A code = **29.1% of the game's code-bearing bytes** — `LEVEL01..LEVEL10/LEVEL.BIN`
and `LEVEL1.BIN` (20), plus `BITS/MEMORY.BIN`. (A 22nd overlay-NAMED file, `LEVEL00/LEVEL.BIN`, is a
4-byte placeholder holding no code — not scannable, not counted.) Of 274 disc files scanned, exactly 23
contain a `jr $ra`: those 21, the boot executable, and `FMV/FMV.BIN` (68 of them). Later instruction
and live evidence resolved the last file: the executable loads it at `0x800D5D20` and immediately calls
`0x800D6628`, offset `0x908` into the freshly loaded bytes. It is therefore a 22nd loaded code module,
although its mixed 510,960-byte file must not be folded wholesale into the old code-byte percentage. Every
file other than those 23 has zero. 67.9–83.4% of each `LEVEL*.BIN` module's `j`/`jal` targets land inside
the boot exe's `.text` (the boot exe's own figure is 91.5%), so these modules are statically linked
against the engine and call into it — that range covers the 16 `LEVEL*.BIN` files with any `j`/`jal`; the
4 stub `LEVEL1.BIN` files have zero (no evidence), and `BITS/MEMORY.BIN` reads **50.6%**, its own figure,
below the LEVEL band though nothing like the 0% of the `PATH*.BIN` trap case.

This is the Vagrant Story answer, not the Mega Man X4 answer, and the consequences are structural:

- **`RE-03` is instruction-verified, not fitted.** The retail caller chooses exactly one of
  `level.bin`/`level1.bin`/`level2.bin`/`level3.bin`, calls one fixed-slot wrapper, and that wrapper
  forms `0x800D12C0`. The 19,040-byte span to the next slot equals the largest LEVEL module and 5/10
  LEVEL+LEVEL1 pairs cannot fit together, so they are alternative contents of one slot.
- **BITS/MEMORY is simultaneously resident at `0x800D5D20`.** The same call path loads it first, the
  retail loader preserves bytes beyond the exact 63,312-byte file size during sector reads, and its
  caller advances the arena to `0x800E54F8`. Reproduce all shipping comparisons and the forced
  opposite answer with `uv run --frozen python tools/overlay_map.py --check` and `--selftest`.
- **`0x800D1000` remains only the old 4 KiB fit.** It is useful corroboration but is not wired. The
  exact instruction-derived slot is authoritative. RE-02 has now consumed it; pinned psxport
  `3418a79b` advances through the resolved CDC pacing dependency to the later
  BITS/MEMORY loader/memory-store boundary.

## The rules that bite hardest here

**Never guess a guest address or an overlay load base.** An un-RE'd legacy `GameConfig` fact stays `0` with a
TODO naming the frontier step. Zero is honest and psxport fails fast on it; a plausible wrong value
breaks boot in a way that reads as a framework bug. An overlay is keyed BY its load address, so a wrong
base emits a whole module of correctly-decoded instructions at wrong addresses — and the emit SUCCEEDS.

**Never import a PC-, N64- or Dreamcast-derived fact.** TS2 shipped on four platforms and the modding
community lives on the **PC** build; almost every TS2 tool and note you will find describes a different
platform's data. `.NGN` — the container the largest TS2 tool in existence is built around — **is not on
the PSX disc** (measured: 0 `grep -ci NGN` hits over the **300** entries `uv run --frozen python tools/discdump.py list`
returns — the tree's own instrument, and its own printed denominator; an earlier "303" was a `wc -l` of
that listing including its blank line and two `[discdump]` trailer lines). A PC-derived struct offset silently imported
here would look exactly like a working fact. `docs/references.md` labels every source with its platform.

**Work the step `re_frontier.py next` names, not a downstream one.** The cardinal sin on a port is
faking a step's output before its RE is done; it makes a broken port look finished.

**Provision from your own disc; commit nothing derived from it.** Resolution order (one implementation,
`tools/resolve_disc.py`): CLI arg > `$PSXPORT_TS2_DISC` > `.env` > a `*.chd` in the repo root. `.env` is
gitignored because the path is machine-specific; `.env.example` is the template.
`uv run --frozen python tools/extract_exe.py` extracts and identity-checks against `docs/info/exe-identity.txt` — note
that this expectation is OUR OWN measurement, not an independent witness, because no decomp states a
target hash for this executable. `tools/go_public.py` audits the full history before this repo is
published.

**Everything transient goes in the gitignored `scratch/`, split by kind** (`scratch/bin/toystory2/`,
`scratch/flat/`, `scratch/raw/`, `scratch/logs/`, `scratch/saves/`). **Never `/tmp`** — a small
RAM-backed tmpfs on this machine; diagnose "disk quota exceeded" with `quota -s`, not `df`.

**A diagnostic that can print nothing is lying.** Before writing a check, write what its NEGATIVE prints:
it carries its denominator and its blind spots. Refuse (non-zero) rather than return empty on a missing
corpus. Every tool here ships a `--selftest` that gates BOTH classes. **And decode MIPS PER WORD, never
with `capstone.md.disasm()` over a whole `.text`** — it stops dead at the first undecodable word and
silently scans only a prefix, which already produced one confident false negative on this game's central
question (`docs/issues/0002-*`).

## Where the framework source comes from — `external/psxport` is the shared tree

`external/psxport` is **not a submodule** (2026-08-16): it is a SYMLINK to the workspace's shared
framework clone (`$PSX/psxport`) when one exists, or a private clone at this repo's `psxport.pin` on a
fresh machine. `tools/psxport_sync.py --auto` (called by `run.sh`) establishes whichever applies. A
framework edit made through either path is the SAME directory, live in every port at once — commit and
push framework work in `psxport/`, never here. `psxport.pin` records the framework commit this game
was built and VERIFIED against; `tools/psxport_sync.py --bump` updates it, and the gate's `--check`
fails when the framework you built against is not the recorded one.

Build against in-progress framework work:

```sh
cmake -S . -B build -DPSXPORT_DIR=/path/to/psxport      # or just ./run.sh — it resolves external/psxport
```

`PSXPORT_DIR` defaults to `external/psxport`, so a bare clone of this repo builds standalone — keep it
that way, and never re-spell `external/psxport` at a call site in cmake or in a tool.
