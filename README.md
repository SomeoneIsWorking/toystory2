# Toy Story 2 — a PC-native port

A PC-native port of **Disney/Pixar Toy Story 2: Buzz Lightyear to the Rescue!** (PS1, USA,
`SLUS_008.93`, Traveller's Tales) built on the
[psxport](https://github.com/SomeoneIsWorking/psxport) static-recompilation framework, vendored here as
`external/psxport`.

## Status: generated boot reaches the CD boundary

The real `toystory2_port` binary now contains the identity-checked executable plus all 21 measured code
modules. Generated crt0 matches the independent CPU oracle in all 34 state fields at its first call;
live boot registers the interrupt chain, opens the CHD, and services stock-libcd command and sector
results. RE-04's first missing state was measured and fixed generically: pinned psxport `3418a79b`
paces each following sector in deterministic guest cycles instead of exposing it immediately. The
default route advances through eleven real ReadN phases into the emitted BITS/MEMORY overlay path.
What exists:

- disc → executable provisioning from **your own** disc image (nothing game-derived is in this repo),
- the framework seam (`GameConfig` / `GameHooks` / generated registry) compiling against psxport, with the
  complete RE-01 boot group and RE-03's two instruction-verified overlay slots wired while every
  un-RE'd callable guest address remains honestly `0`,
- the symbolic crt0 verifier plus the code/overlay census, exact overlay-loader verifier, `.RAW` container
  probe, disc extractor, stock-libcd command/trace verifier, and two-method Ghidra xref gate, each
  validated in both directions,
- the project registries (`docs/re-frontier.md`, `docs/codemap.md`, `docs/references.md`, `docs/info/`,
`docs/issues/`), plus a forced-negative generated/oracle boundary gate.

`docs/codemap.md` is the honest inventory; `docs/re-frontier.md` is the ordered RE chain. RE-00, RE-01
and RE-03 are re-verified; RE-02 is partial; CD, frame, input, remaining HLE and assets remain open.

**Two facts that shape everything here:**

1. **There is no decompilation of this game.** Unlike psxport's other consumers there is no symbol map,
   no function boundaries and no matching build to check against — every address must come from
   reproducible binary evidence on this executable. `docs/references.md` records how that negative was established and why a search
   negative is weaker than a measurement.
2. **This game streams code overlays.** 21 files hold 29.1% of its code-bearing bytes. RE-03 proves
   that all LEVEL variants share the slot at `0x800D12C0` and BITS/MEMORY occupies the simultaneous
   slot at `0x800D5D20`; the authoritative emitter includes exactly those 21 modules.

## Getting started

```sh
git clone <this repo> && cd toystory2
python3 tools/psxport_sync.py --auto              # shared workspace checkout or private clone at psxport.pin
cp .env.example .env && $EDITOR .env                # point it at your own disc image (.env is gitignored)
./run.sh                                             # focused provisioning, emit, Clang build, launch
python3 tools/recomp_substrate.py --selftest         # identity/map/emitter forced-negative gate
python3 tools/code_scan.py --census scratch/flat --ctrl-exe scratch/bin/toystory2/SLUS_008.93
python3 tools/base_fit.py --selftest                # re-derive the overlay slot from the bytes
python3 tools/ram_image.py                          # header-driven 2 MiB image + placement manifest
external/psxport/tools/decomp.sh import scratch/ghidra/ram-boot.bin ts2boot
python3 tools/re_xref.py --selftest                 # prove Ghidra and the independent fold both answer
python3 tools/verify_crt0.py --check                # re-derive all shipping RE-01 fields from instructions
python3 tools/overlay_map.py --check                # re-derive both shipping RE-03 slots from instructions
python3 tools/verify_cd_command.py --selftest       # derive libcd ABI; force bounded/runaway answers
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target toystory2_port toystory2_recomp_boundary -j"$(nproc)"
python3 tools/compare_recomp_boundary.py --selftest \
  --oracle build/psxport_build/tools/oracle/oracle_trace \
  --runner scratch/bin/toystory2_recomp_boundary
ctest --test-dir build --output-on-failure         # launcher, RE-01/03/04, oracle, format, tidy
python3 tools/re_frontier.py next                   # what to work on
```

`tools/psxport_sync.py --auto` is the single framework-resolution path. In the workspace it makes
`external/psxport` a symlink to the one shared framework checkout; in a standalone clone it creates a
private checkout at `psxport.pin` and initializes the required framework vendors non-recursively.

`run.sh` is the stable launcher wrapper over `tools/run.py`; Python owns focused provisioning,
hash-checked regeneration, the Clang build, RmlUi asset resolution, and launching the current product.

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
