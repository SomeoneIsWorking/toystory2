# Toy Story 2 native/dynarec port

This repository targets the USA PlayStation release of *Toy Story 2: Buzz Lightyear to the Rescue!*
(`SLUS_008.93`). It combines title-owned native subsystems with psxport's runtime Lightrec executor;
the authenticated user-supplied game image remains data.

## Current state

The durable feature inventory and exact gaps are in [project-state](docs/project-state.md), and the
open product bugs are in [docs/issues](docs/issues).

The Linux x86-64 product links against psxport's maintained Lightrec executor and runs the title,
menu, level select, LEVEL01 and Andy's Room through it. Native boundary tests pass; the resident
picture, the 24-bit movies and true widescreen remain open. No offline guest source or
player-selectable interpreter path is present.

## Build and run

Provide the original disc through `PSXPORT_TS2_DISC`, `.env`, an unambiguous repository-root CHD, or
the optional positional argument:

```sh
./run.sh [/path/to/game.chd]
```

`run.sh` is only the launcher. It resolves psxport, configures `build/player`, builds
`build/player/bin/toystory2_port`, and starts that product. It does not run tests or generate guest
code.

The gate is build + the product's hermetic C++ boundary tests + the executable's own help contract +
clang-format/clang-tidy/cpp_policy:

```sh
CXX=clang++ CC=clang CMAKE_BUILD_PARALLEL_LEVEL=6 uv run --frozen python tools/verify.py
```

`tools/verify.py` uses psxport's shared consumer verifier to configure the Ninja
`build/verify` tree, build the product, run that CTest suite, and inspect the linked execution
boundary. Maintained dependency overrides use `PSXPORT_LIGHTREC_DIR` and
`PSXPORT_LIGHTNING_PREFIX`, matching the framework verifier. This asset-free check does not establish
gameplay compatibility.

To look at the game, drive it, or capture frames, use the maintainer tools: `tools/headless_run.py`
(one bounded headless run, with `--tap`, `--shot-at`, `--aspect`, `--dump-at`), `tools/ts2_route.py`
and `tools/verify_route.py` (exact-frame pad routes), and `tools/verify_movement.py` (gameplay judged
from guest RAM). None of them are part of the gate.

Game files, extracted executables, build products, and runtime captures are not tracked or packaged.
