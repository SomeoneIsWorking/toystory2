# Toy Story 2 — a PC-native port (bootstrap stage)

A PC-native port of **Disney/Pixar Toy Story 2: Buzz Lightyear to the Rescue!** (PS1, USA,
`SLUS_008.93`, Traveller's Tales) built on the
[psxport](https://github.com/SomeoneIsWorking/psxport) static-recompilation framework, vendored here as
`external/psxport`.

## Status: scaffolding. It does not run

Created 2026-08-12. There is no recompiled substrate, no port binary, and nothing about the game is
reverse-engineered yet. What exists:

- disc → executable provisioning from **your own** disc image (nothing game-derived is in this repo),
- the framework seam (`GameConfig` / `GameHooks`) compiling against the pinned framework, with every
  guest address honestly `0`,
- four measured instruments — the code/overlay census, the overlay load-base fit, the `.RAW` container
  probe, and the disc extractor — each with a `--selftest` that gates both classes,
- the project registries (`docs/re-frontier.md`, `docs/codemap.md`, `docs/references.md`, `docs/info/`,
  `docs/issues/`).

`docs/codemap.md` is the honest inventory; `docs/re-frontier.md` is the ordered RE chain and **every
entry in it is `todo` or `blocked` — none is re-verified.**

**Two facts that shape everything here:**

1. **There is no decompilation of this game.** Unlike psxport's other consumers there is no symbol map,
   no function boundaries and no matching build to check against — every address comes out of Ghidra on
   this executable. `docs/references.md` records how that negative was established and why a search
   negative is weaker than a measurement.
2. **This game streams code overlays.** 21 files hold 29.1% of its code-bearing bytes, so the
   recompiler cannot emit anything until their load bases are confirmed (`docs/re-frontier.md` RE-03).

## Getting started

```sh
git clone <this repo> && cd toystory2
git submodule update --init external/psxport                                      # the one submodule
git -C external/psxport submodule update --init vendor/lucent vendor/beetle-psx    # and psxport's own
cp .env.example .env && $EDITOR .env                # point it at your own disc image (.env is gitignored)
python3 tools/extract_exe.py                        # extract + identity-check SLUS_008.93
python3 tools/extract_disc_files.py                 # the flat scan corpus in scratch/flat/ (~25 MiB)
python3 tools/code_scan.py --census scratch/flat --ctrl-exe scratch/bin/toystory2/SLUS_008.93
python3 tools/base_fit.py --selftest                # re-derive the overlay slot from the bytes
cmake -S . -B build && cmake --build build --target toystory2_seam -j"$(nproc)"
python3 tools/re_frontier.py next                   # what to work on
```

**The second line is not optional and `--recurse-submodules` does not replace it.** psxport's own
`vendor/lucent` (the logger) and `vendor/beetle-psx/deps/libchdr` (CHD access) are what `cmake` and the
provisioning tools need, and initialising `external/psxport` alone leaves them empty:

- `git clone --recurse-submodules` (of this repo or of psxport) **aborts** with `fatal: No url found for
  submodule path 'vendor/beetle-psx/deps/lightning/gnulib'` and leaves `vendor/lucent` empty,
- `external/psxport/scripts/sync-submodules.sh` prints *"all at this repo's recorded gitlinks"* while
  `vendor/lucent` is still empty — it certifies pins it never reached
  (`external/psxport/docs/workspace/KNOWN-DEFECT-sync-submodules.md`),
- so the **per-path, non-recursive** form above is the one that works. It sidesteps the url-less
  `gnulib` path entirely. `CMakeLists.txt` checks for both vendors and fails with that exact command
  rather than letting an `add_subdirectory` error surface from inside the framework.

`run.sh` is the eventual play launcher; today it does every real step and then stops, naming what
blocks the recompile.

## Requirements

cmake ≥ 3.21, pkg-config, SDL3, zlib, zstd, python3, a C++20 toolchain.

## Legal

**No game content is distributed here.** The disc image, the executable extracted from it, the assets
extracted from it and the recompiled substrate derived from it are all yours and are gitignored.
`tools/go_public.py` audits the full history for disc-derived material and machine-specific paths before
this repo is ever published.

No third-party code is vendored in this repo beyond the framework itself. `docs/references.md` records
the external RE and format work that exists for this engine **with its licences** — four of the six
relevant projects carry no licence at all, i.e. all rights reserved, and are read-only.
