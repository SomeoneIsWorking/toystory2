# Toy Story 2 — a PC-native port (bootstrap stage)

A PC-native port of **Disney/Pixar Toy Story 2: Buzz Lightyear to the Rescue!** (PS1, USA,
`SLUS_008.93`, Traveller's Tales) built on the
[psxport](https://github.com/SomeoneIsWorking/psxport) static-recompilation framework, vendored here as
`external/psxport`.

## Status: scaffolding. It does not run

There is no recompiled substrate or port binary. RE-00 supplies the verified Ghidra project, and RE-01
now proves the complete crt0 boot group directly from the executable's instructions. What exists:

- disc → executable provisioning from **your own** disc image (nothing game-derived is in this repo),
- the framework seam (`GameConfig` / `GameHooks`) compiling against the pinned framework, with only the
  complete RE-01 boot group wired and every other un-RE'd guest address honestly `0`,
- the symbolic crt0 verifier plus the code/overlay census, overlay load-base fit, `.RAW` container
  probe, disc extractor, and two-method Ghidra xref gate, each validated in both directions,
- the project registries (`docs/re-frontier.md`, `docs/codemap.md`, `docs/references.md`, `docs/info/`,
  `docs/issues/`).

`docs/codemap.md` is the honest inventory; `docs/re-frontier.md` is the ordered RE chain. RE-00 and
RE-01 are re-verified; overlays, substrate, CD, frame, input, HLE and assets remain open.

**Two facts that shape everything here:**

1. **There is no decompilation of this game.** Unlike psxport's other consumers there is no symbol map,
   no function boundaries and no matching build to check against — every address must come from
   reproducible binary evidence on this executable. `docs/references.md` records how that negative was established and why a search
   negative is weaker than a measurement.
2. **This game streams code overlays.** 21 files hold 29.1% of its code-bearing bytes, so the
   recompiler cannot emit anything until their load bases are confirmed (`docs/re-frontier.md` RE-03).

## Getting started

```sh
git clone <this repo> && cd toystory2
python3 tools/psxport_sync.py --auto              # shared workspace checkout or private clone at psxport.pin
cp .env.example .env && $EDITOR .env                # point it at your own disc image (.env is gitignored)
python3 tools/extract_exe.py                        # extract + identity-check SLUS_008.93
python3 tools/extract_disc_files.py                 # the flat scan corpus in scratch/flat/ (~25 MiB)
python3 tools/code_scan.py --census scratch/flat --ctrl-exe scratch/bin/toystory2/SLUS_008.93
python3 tools/base_fit.py --selftest                # re-derive the overlay slot from the bytes
python3 tools/ram_image.py                          # header-driven 2 MiB image + placement manifest
external/psxport/tools/decomp.sh import scratch/ghidra/ram-boot.bin ts2boot
python3 tools/re_xref.py --selftest                 # prove Ghidra and the independent fold both answer
python3 tools/verify_crt0.py --check                # re-derive all shipping RE-01 fields from instructions
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target toystory2_seam -j"$(nproc)"
ctest --test-dir build --output-on-failure         # launcher, RE-01, format, and tidy gates
python3 tools/re_frontier.py next                   # what to work on
```

`tools/psxport_sync.py --auto` is the single framework-resolution path. In the workspace it makes
`external/psxport` a symlink to the one shared framework checkout; in a standalone clone it creates a
private checkout at `psxport.pin` and initializes the required framework vendors non-recursively.

`run.sh` is the stable four-line launcher wrapper over `tools/run.py`; today the Python launcher does
every real provisioning/build step and then exits 3, naming what blocks the recompile.

## Requirements

cmake ≥ 3.21, pkg-config, SDL3, zlib, zstd, Python 3, Clang/clang++ with clang-format and clang-tidy,
and Ruff for Python formatting/linting.

## Legal

**No game content is distributed here.** The disc image, the executable extracted from it, the assets
extracted from it and the recompiled substrate derived from it are all yours and are gitignored.
`tools/go_public.py` audits the full history for disc-derived material and machine-specific paths before
this repo is ever published.

No third-party code is vendored in this repo beyond the framework itself. `docs/references.md` records
the external RE and format work that exists for this engine **with its licences** — four of the six
relevant projects carry no licence at all, i.e. all rights reserved, and are read-only.
