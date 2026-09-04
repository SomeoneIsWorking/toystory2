# Toy Story 2 native/dynarec port

This repository targets the USA PlayStation release of *Toy Story 2: Buzz Lightyear to the Rescue!*
(`SLUS_008.93`). It combines title-owned native subsystems with psxport's runtime Lightrec executor;
the authenticated user-supplied game image remains data.

## Current state

The former offline translator, emitted guest-source corpus, seed manifest, generated dispatcher, and
their tests have been removed. The native title layer now uses psxport's typed guest-call and
image-scoped override APIs. psxport intentionally returns a named `Lightrec dynarec-only backend is
not linked` fault until its maintained no-interpreter Lightrec dependency is available, so gameplay
is currently blocked at that one executor boundary. There is no static or interpreter fallback.

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
uv run --frozen python tools/test_run.py
uv run --frozen python tools/test_structure.py
uv run --frozen python tools/check_structure.py
```

Game files, extracted executables, build products, and runtime captures are not tracked or packaged.
