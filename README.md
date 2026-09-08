# Toy Story 2 native/dynarec port

This repository targets the USA PlayStation release of *Toy Story 2: Buzz Lightyear to the Rescue!*
(`SLUS_008.93`). It combines title-owned native subsystems with psxport's runtime Lightrec executor;
the authenticated user-supplied game image remains data.

## Current state

The Linux x86-64 product now links against psxport's maintained Lightrec executor. Native boundary
tests pass, but retail boot stops at a bounded platform-initialization call before gameplay. The
exact runtime and remaining verification gaps are recorded in project state below. No offline guest
source or player-selectable interpreter path is present.

The durable feature inventory and exact gaps are in [project-state](docs/project-state.md). The
binary evidence frontier is in [re-frontier](docs/re-frontier.md).

## Build and run

Provide the original disc through `PSXPORT_TS2_DISC`, `.env`, an unambiguous repository-root CHD, or
the optional positional argument:

```sh
./run.sh [/path/to/game.chd]
```

`run.sh` is only the launcher. It resolves psxport, configures `build/player`, builds
`build/player/bin/toystory2_port`, and starts that product. It does not run tests or generate guest
code.

Run the non-launching checks explicitly:

```sh
CC=clang CXX=clang++ uv run --frozen python tools/verify.py
uv run --frozen python tools/test_run.py
uv run --frozen python tools/test_structure.py
uv run --frozen python tools/check_structure.py
```

`tools/verify.py` uses psxport's shared consumer verifier to configure the Ninja
`build/verify` tree, build the product, run the title's complete CTest suite, and inspect the linked
execution boundary. Maintained dependency overrides use `PSXPORT_LIGHTREC_DIR` and
`PSXPORT_LIGHTNING_PREFIX`, matching the framework verifier. This asset-free check does not establish
gameplay compatibility.

Game files, extracted executables, build products, and runtime captures are not tracked or packaged.
