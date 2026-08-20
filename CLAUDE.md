# Toy Story 2 — working rules for THIS repo

A PC-native port of **Disney/Pixar Toy Story 2: Buzz Lightyear to the Rescue! (PS1, USA,
`SLUS_008.93` / SLUS-00893)**, Traveller's Tales, built on the
[psxport](https://github.com/SomeoneIsWorking/psxport) static-recompilation framework
(`external/psxport`). psxport recompiles the game's MIPS code to C and supplies the PSX platform layer;
this repo supplies the game — the seam, the RE, and the native reimplementations.

**The framework rules are NOT restated here. Read `external/psxport/CLAUDE.md`** — it is the authority
for how a game consumes psxport: the CVar ladder, the seam, `generated/` being sacrosanct, RE-first,
diagnostics through `lucent`, the registries, never editing `external/psxport`, and the standing USER
directive that **`./run.sh` is the user's and agents must never invoke it**. The workspace map is
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
this port ever holds will come out of Ghidra on this executable
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

## THE STATE OF THIS PORT: the RE supply works; no game subsystem does

There is **no recompiled substrate, no port binary, and no wired guest address.** RE-00 is the one
completed RE step: a fresh Ghidra 12 import over the measured executable passes the independent xref
gate and emits C for entry `0x80082D60` (`docs/info/claims/008-*`). That proves the decompiler supply,
not crt0, the overlay loader, or boot. `game/core/game_config.cpp` remains all zeros with each field
pointing at its open step in `docs/re-frontier.md`. If something here looks like a running port, check
`docs/codemap.md` — the honest inventory is provisioning, a compiling seam, five instruments, and the
registries.

What DOES build today, and is the gate for a change to the seam:

```sh
python3 tools/psxport_sync.py --auto                                    # resolve external/psxport (symlink to the shared clone, or a private clone at psxport.pin)
cmake -S . -B build && cmake --build build --target toystory2_seam -j$(nproc)
```

**`external/psxport` is NOT a submodule any more (2026-08-16)** — it is a symlink to the workspace's
shared framework clone when one exists, or a private clone at this repo's `psxport.pin` on a fresh
machine; `tools/psxport_sync.py --auto` establishes whichever applies. The framework's own nested
vendors (`vendor/lucent`, `vendor/beetle-psx/deps/libchdr`) are the SHARED clone's concern — initialized
once there, never per-port (`--recursive` aborts on beetle-psx's url-less `deps/lightning/gnulib`, and
`sync-submodules.sh` prints "all at recorded gitlinks" over vendors it never reached —
`external/psxport/docs/workspace/KNOWN-DEFECT-sync-submodules.md`). `CMakeLists.txt` fails with a clear
message if they are missing in the shared clone.
url-less path. `CMakeLists.txt` checks both vendors and fails with that exact command; `tools/discdump.py`
says the same when its build fails.

## Start here, every task

```sh
python3 tools/re_frontier.py next            # which RE step is actually ready to work
python3 tools/info.py brief <words>          # what's already proven — and does it still hold?
python3 tools/catalog.py search <symptom>    # has this been hit (or ruled out) before?
```

Believe these over your instinct about what is known. End the task by writing back what you proved, what
you disproved, and any tool you caught lying. `tools/re_frontier.py` is a SHIM onto the shared engine in
`external/psxport/tools/port/`; do not grow a local copy of it.

## THE DEFINING STRUCTURAL FACT: this game STREAMS CODE OVERLAYS

**Measured 2026-08-12, four independent signals** (`docs/info/claims/002-*`, `003-*`). **21 files** hold
245,148 bytes of R3000A code = **29.1% of the game's code-bearing bytes** — `LEVEL01..LEVEL10/LEVEL.BIN`
and `LEVEL1.BIN` (20), plus `BITS/MEMORY.BIN`. (A 22nd overlay-NAMED file, `LEVEL00/LEVEL.BIN`, is a
4-byte placeholder holding no code — not scannable, not counted.) Of 274 disc files scanned, exactly 23
contain a `jr $ra`: those 21, the boot executable, and **`FMV/FMV.BIN` (68 of them) — whose class is
UNRESOLVED** (`docs/re-frontier.md` RE-04) and which is therefore **not** part of the overlay set. Every
file other than those 23 has zero. 67.9–83.4% of each `LEVEL*.BIN` module's `j`/`jal` targets land inside
the boot exe's `.text` (the boot exe's own figure is 91.5%), so these modules are statically linked
against the engine and call into it — that range covers the 16 `LEVEL*.BIN` files with any `j`/`jal`; the
4 stub `LEVEL1.BIN` files have zero (no evidence), and `BITS/MEMORY.BIN` reads **50.6%**, its own figure,
below the LEVEL band though nothing like the 0% of the `PATH*.BIN` trap case.

This is the Vagrant Story answer, not the Mega Man X4 answer, and the consequences are structural:

- **`RE-03` (overlay load bases) is a REAL open step, not a skip.** `emit.py` treats a missing overlay
  base as a hard error, deliberately, so there is no recompiling anything until it lands.
- **A base is MEASURED but is NOT a resident base.** `tools/base_fit.py` fits `0x800D1000` for 15
  modules (75–100% of their own out-of-`.text` jals, 0.0% runner-up), and the modules' own trailer tables
  hold absolute `0x800Dxxxx` pointers. But the constant appears NOWHERE in the boot exe — not as a
  lui/addiu pair (12,670 pairs folded over all 148,992 `.text` words) and not as a literal word — so how
  the loader arrives at it is unknown. **Never paste it into `overlaySlots` or `overlay_bases`.**
- **Whether `LEVEL.BIN` and `LEVEL1.BIN` share one slot or occupy two is UNRESOLVED** and both readings
  fit the bytes. It decides the memory map, so it is part of RE-03 and not an afterthought.
- One ready code step is a decompile of the function referencing the overlay-name string group at VA
  `0x80022F84`..`0x80022FA8` — a 12-byte-stride table holding `level1.bin` at `0x80022F84`, `level2.bin`
  at `0x80022F90`, `level3.bin` at `0x80022F9C` and **`level.bin` itself at `0x80022FA8`**. Xref both
  ends; `0x80022F84` is the table base, NOT the `level.bin` literal. A decompiler job, per the RE-first
  rule — not a grep job and not more statistics. RE-01 (the entry/crt0 group) is independently ready;
  `python3 tools/re_frontier.py next` is the authority when choosing between them.

## The rules that bite hardest here

**Never guess a guest address or an overlay load base.** An un-RE'd `GameConfig` field stays `0` with a
TODO naming the frontier step. Zero is honest and psxport fails fast on it; a plausible wrong value
breaks boot in a way that reads as a framework bug. An overlay is keyed BY its load address, so a wrong
base emits a whole module of correctly-decoded instructions at wrong addresses — and the emit SUCCEEDS.

**Never import a PC-, N64- or Dreamcast-derived fact.** TS2 shipped on four platforms and the modding
community lives on the **PC** build; almost every TS2 tool and note you will find describes a different
platform's data. `.NGN` — the container the largest TS2 tool in existence is built around — **is not on
the PSX disc** (measured: 0 `grep -ci NGN` hits over the **300** entries `python3 tools/discdump.py list`
returns — the tree's own instrument, and its own printed denominator; an earlier "303" was a `wc -l` of
that listing including its blank line and two `[discdump]` trailer lines). A PC-derived struct offset silently imported
here would look exactly like a working fact. `docs/references.md` labels every source with its platform.

**Work the step `re_frontier.py next` names, not a downstream one.** The cardinal sin on a port is
faking a step's output before its RE is done; it makes a broken port look finished.

**Provision from your own disc; commit nothing derived from it.** Resolution order (one implementation,
`tools/resolve_disc.py`): CLI arg > `$PSXPORT_TS2_DISC` > `.env` > a `*.chd` in the repo root. `.env` is
gitignored because the path is machine-specific; `.env.example` is the template.
`python3 tools/extract_exe.py` extracts and identity-checks against `docs/info/exe-identity.txt` — note
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
