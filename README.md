# Toy Story 2 — a PC-native port

A PC-native port of **Disney/Pixar Toy Story 2: Buzz Lightyear to the Rescue!** (PS1, USA,
`SLUS_008.93`, Traveller's Tales) built on the
[psxport](https://github.com/SomeoneIsWorking/psxport) static-recompilation framework, vendored here as
`external/psxport`.

## Status: generated boot renders boot cards and enters FMV

The real `toystory2_port` binary contains the identity-checked executable plus all 22 proven loaded code
modules. Generated crt0 matches the independent CPU oracle in all 34 state fields at its first call.
Live boot services stock libcd, renders the legal and ESRB cards through the guest picture, enters emitted
`FMV/FMV.BIN` code, then stops honestly at the next shared-runtime boundary: BIOS `A0:0x25`.
What exists:

- disc → executable provisioning from **your own** disc image (nothing game-derived is in this repo),
- a process-lifetime `ToyStory2Runtime` derived owner plus the generated registry, with immutable
  RE-01/RE-03 facts and unresolved compatibility callbacks temporarily bounded behind
  `LegacyGameRuntimeAdapter`,
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
uv run --frozen python tools/psxport_sync.py --auto # shared workspace checkout or private clone at psxport.pin
cp .env.example .env && $EDITOR .env                # point it at your own disc image (.env is gitignored)
./run.sh                                             # focused provisioning, emit, native build, launch
./run.sh --prepare-only                              # same cold path without opening the game
uv run --frozen python tools/recomp_substrate.py --selftest
uv run --frozen python tools/code_scan.py --census scratch/flat --ctrl-exe scratch/bin/toystory2/SLUS_008.93
uv run --frozen python tools/base_fit.py --selftest # re-derive the overlay slot from the bytes
uv run --frozen python tools/ram_image.py           # header-driven 2 MiB image + placement manifest
external/psxport/tools/decomp.sh import scratch/ghidra/ram-boot.bin ts2boot
uv run --frozen python tools/re_xref.py --selftest  # prove Ghidra and the independent fold both answer
uv run --frozen python tools/verify_crt0.py --check
uv run --frozen python tools/overlay_map.py --check
uv run --frozen python tools/verify_cd_command.py --selftest
uv run --frozen python tools/verify_model_table_reset.py --selftest
# Maintainer evidence after a bounded PSXPORT_WWATCH=800C728C,800C7290 run:
uv run --frozen python tools/verify_model_table_reset.py --check-log scratch/logs/re16-reset-fixed.log
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DPython3_EXECUTABLE="$(uv run --frozen python -c 'import sys; print(sys.executable)')"
cmake --build build --target toystory2_port toystory2_recomp_boundary -j"$(nproc)"
uv run --frozen python tools/compare_recomp_boundary.py --selftest \
  --oracle build/psxport_build/tools/oracle/oracle_trace \
  --runner scratch/bin/toystory2_recomp_boundary
ctest --test-dir build --output-on-failure         # launcher, RE-01/03/04, oracle, format, tidy
uv run --frozen python tools/re_frontier.py next   # what to work on
```

`tools/psxport_sync.py --auto` is the single framework-resolution path. In the workspace it makes
`external/psxport` a symlink to the one shared framework checkout; in a standalone clone it creates a
private checkout at `psxport.pin` and initializes the required framework vendors non-recursively.

`run.sh` is the stable launcher wrapper over the frozen `uv.lock` environment and `bootstrap.py`;
Python owns focused provisioning, hash-checked regeneration, CMake build policy, RmlUi asset resolution,
and the current windowed product. Its isolated `scratch/build/player` tree builds only
`toystory2_port` with `BUILD_TESTING=OFF`; CTest remains a separate maintainer gate in `build/`.
Explicit `CC`/`CXX` values pass through unchanged, and otherwise CMake discovers the host toolchain
without compiler identity filtering.

## Requirements

`uv`, cmake ≥ 3.21, `glslc`, pkg-config, SDL3, SDL3_image, FreeType, zlib, zstd, and a C/C++ toolchain
CMake can use. Maintainer verification separately uses Clang, clang-format, and clang-tidy. The
launcher names the exact supported Homebrew, APT, DNF, or Windows install command when a packaged
dependency is absent; it never runs a privileged package manager itself.

## Legal

**No game content is distributed here.** The disc image, the executable extracted from it, the assets
extracted from it and the recompiled substrate derived from it are all yours and are gitignored.
`tools/go_public.py` audits the full history for disc-derived material and machine-specific paths before
this repo is ever published.

No third-party code is vendored in this repo beyond the framework itself. `docs/references.md` records
the external RE and format work that exists for this engine **with its licences** — four of the six
relevant projects carry no licence at all, i.e. all rights reserved, and are read-only.
