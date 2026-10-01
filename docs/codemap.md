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
| Session ownership | Own one boot-to-exit run: install the runtime, self-provision and load the authenticated executable, initialize platform services, register native overrides, enter the native boot, and tear everything down by destruction | `game/core/title_session.*` | `ts2::TitleSession`, `ts2::kDefaultExe`, `ts2::kDiscExePath` | `CLAUDE.md` |
| Process entry point | Parse `--help` and the optional executable path, construct one session, return its exit code | `game/core/main.cpp` | `main` | `CLAUDE.md` |
| Title runtime | Compose per-Core title context, frame driver, native overrides and render capabilities | `game/core/toystory2_runtime.*`, `game/core/toystory2_context.*` | `ToyStory2Runtime` | `CLAUDE.md` |
| Guest execution adapter | Centralize typed guest calls, bounded finite initialization continuation, scoped original calls and resident override registration | `game/core/guest_execution.*` | `callGuestToReturn`, `callFiniteGuestToReturn`, `executeFiniteGuestCall`, `ResumableGuestCall`, `installResidentOverride` | `CLAUDE.md` |
| Overlay code slots (LEVEL at `0x800D12C0`, shared MEMORY/FMV at `0x800D5D20`) | Observe the measured file loader, authenticate each retail source before transfer and its loaded bytes afterward, retire replaced identity, invalidate translations over the slot window, and publish one image generation per slot | `game/overlay/overlay_slot.*` (one slot), `overlay_images.*` (both slots + loader observer), `level_slot_image.h`, `shared_slot_image.h` (retail module tables) | `installOverlayLoadObserver`, `OverlaySlot::publish` | `docs/issues/0020` |
| Exact title facts | Hold verified executable, memory, HLE, CD, DMA, pad and projection facts as the typed objects `ToyStory2Runtime` hands the framework | `game/core/guest_facts.h`, `game/cd/` | `ts2::facts::kProgramImage`, `kPlatformHlePlan`, `kCdStreamCallbackLayout` | `tools/overlay_map.py` |
| STR completion ring | The FMV wait's ring state words and the two guards that gate the only routine that can satisfy it; each address re-derived from the retail bytes | `game/cd/str_completion_layout.h` | `ts2::cd::kStrCompletionLayout` | `docs/issues/0027` |
| Boot synchronization | Preserve measured graphics state without guest-owned timing | `game/boot/` | `initializeGuestMain`, `installNativeSyncOverrides` | `docs/issues/0025` |
| Frame orchestration | Own bounded front-end, intro-movie (one display field per step through `ResumableGuestCall`), transition and resident sequencing | `game/loop/` | `createFrameDriver`, `stepOuterLoop` | `docs/issues/0025` |
| Native input | Publish measured pad packets and title-visible state | `game/input/` | `installNativePadOverrides` | `docs/issues/0020` |
| Rendering | Publish authored projection, constrain widescreen behavior, capture authored camera/visibility/mesh inputs, and decode resident mesh commands | `game/render/` | `guestWidescreenPolicy`, `installResidentSceneObservationOverrides` | `docs/issues/0030-native-scene-producers-are-not-grounded-at-the-p.md` |
| Binary and asset tools | Derive title facts from authenticated game bytes | `tools/` | individual Python CLIs | `CLAUDE.md` |
| Hermetic boundaries | Verify title-owned CD, projection, finite-frame, and image/execution contracts without gameplay | `tests/` | CTest targets | `README.md` |
| Product verification | Supply title targets to the framework's shared configure/build/test and execution-boundary verifier | `tools/verify.py` | `main` | `README.md` |
| Gameplay control tools | Exact-frame pad routes, Buzz's guest-RAM position words with their cited instructions, the movement judge, and the control-channel ledger reader | `tools/ts2_route.py`, `tools/headless_run.py`, `tools/ts2_guest_words.py`, `tools/verify_route.py`, `tools/verify_movement.py`, `tools/execution_ledger.py` | `verify_route.py --route/--negative/--determinism`, `verify_movement.py --run/--negative` | `README.md` |
| PSX platform | Own Lightrec, CPU state, memory, invalidation, native dispatch, hardware and presentation | `external/psxport/` | `psx::cpu::dispatchGuest` | `external/psxport/AGENTS.md` |

## Where new work goes

- CPU decoding, lowering, cache, invalidation and executor exits: `external/psxport/runtime/cpu/`.
- Title-specific native behavior or override policy: the smallest cohesive module under `game/`.
- Environment/CLI/settings ingestion: psxport's configuration API or the Python launcher; never a
  title subsystem-local `getenv`.
- Product diagnostics: Lucent at the owning call site.
- Offline evidence extraction: a modular Python tool under `tools/`; never executable guest source.
- A measurement, census, or claim-recording tool has no home here. The gate is the product build, its
  unit tests, the C++ policy checks and the pin check; anything else belongs in psxport or nowhere.
