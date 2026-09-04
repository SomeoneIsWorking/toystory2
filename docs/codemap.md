# Codemap

Toy Story 2 owns title identity, native behavior, and enhancement policy. `external/psxport` owns PSX
hardware and the dynarec runtime. Dependencies point from the title into psxport; psxport never names
Toy Story 2.

```text
run.sh -> bootstrap.py -> tools/run.py -> CMake product
                                         |
                                         v
ToyStory2Runtime -> native title owners -> guest_execution -> psxport Lightrec executor
                                         |
                                         +-> image-scoped native dispatcher
```

| Subsystem | Responsibility | Current/target location | Entry point | Deep doc |
|---|---|---|---|---|
| Launcher | Resolve psxport, configure `build/player`, build and start the product | `run.sh`, `bootstrap.py`, `tools/run.py` | `tools.run.main` | `README.md` |
| Build graph | Compose title source with psxport; never generate guest code | `CMakeLists.txt`, `cmake/toystory2_port.cmake` | `toystory2_port` | `CLAUDE.md` |
| Product composition | Install runtime, load authenticated executable, initialize platform services | `game/core/main.cpp` | `main` | `CLAUDE.md` |
| Title runtime | Compose per-Core title context, frame driver, native overrides and render capabilities | `game/core/toystory2_runtime.*`, `game/core/toystory2_context.*` | `ToyStory2Runtime` | `CLAUDE.md` |
| Guest execution adapter | Centralize typed guest calls, scoped original calls and resident override registration | `game/core/guest_execution.*` | `callGuestToReturn`, `installResidentOverride` | `CLAUDE.md` |
| Exact title facts | Hold verified executable, memory, HLE, CD, pad and projection facts pending typed extraction | `game/core/game_config.cpp`, `game/core/game_hooks.cpp`, `game/cd/` | `legacy::measuredConfig` | `docs/re-frontier.md` |
| Boot synchronization | Preserve measured graphics state without guest-owned timing | `game/boot/` | `initializeGuestMain`, `installNativeSyncOverrides` | `docs/re-frontier.md` |
| Frame orchestration | Own bounded front-end, transition and resident sequencing | `game/loop/` | `createFrameDriver` | `docs/re-frontier.md` |
| Native input | Publish measured pad packets and title-visible state | `game/input/` | `installNativePadOverrides` | `docs/re-frontier.md` |
| Rendering | Publish authored projection, constrain widescreen behavior, capture authored camera/visibility/mesh inputs, and decode resident mesh commands | `game/render/` | `guestWidescreenPolicy`, `installResidentSceneObservationOverrides` | `docs/issues/0030-native-scene-producers-are-not-grounded-at-the-p.md` |
| Binary and asset tools | Derive title facts from authenticated game bytes | `tools/` | individual Python CLIs | `docs/re-frontier.md` |
| Structure policy | Reject retired execution artifacts, direct product stderr, stray environment reads and monolith growth | `tools/structure/`, `tools/check_structure.py` | `scan_repository` | `CLAUDE.md` |
| Hermetic boundaries | Verify title-owned CD, projection, and finite-frame contracts without gameplay | `tests/` | CTest targets | `README.md` |
| PSX platform | Own Lightrec, CPU state, memory, invalidation, native dispatch, hardware and presentation | `external/psxport/` | `psx::cpu::dispatchGuest` | `external/psxport/AGENTS.md` |

## Where new work goes

- CPU decoding, lowering, cache, invalidation and executor exits: `external/psxport/runtime/cpu/`.
- Title-specific native behavior or override policy: the smallest cohesive module under `game/`.
- Environment/CLI/settings ingestion: psxport's configuration API or the Python launcher; never a
  title subsystem-local `getenv`.
- Product diagnostics: Lucent at the owning call site.
- Offline evidence extraction: a modular Python tool under `tools/`; never executable guest source.
