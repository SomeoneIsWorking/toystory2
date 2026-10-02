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
| Guest execution adapter | Centralize typed guest calls, bounded finite initialization continuation, scoped original calls and resident override registration | `game/core/guest_execution.*` | `callGuestToReturn`, `callFiniteGuestToReturn`, `executeFiniteGuestCall`, `ResumableGuestCall` (a call spanning display fields, or yielding one host slice per native movie frame), `installResidentOverride` | `CLAUDE.md` |
| Movie playback | Replace the FMV overlay's own streaming player with psxport's native STR player, scoped to the FMV image generation, answering exactly what the retail player answers (0 at end, the cold-start word on a skip) | `game/fmv/guest_movie_player.*` | `fmv::installGuestMoviePlayer` | `docs/re-frontier.md` RE-19, `docs/issues/0022`, `docs/issues/0040` |
| Overlay code slots (LEVEL at `0x800D12C0`, shared MEMORY/FMV at `0x800D5D20`) | Observe the measured file loader, authenticate each retail source before transfer and its loaded bytes afterward, retire replaced identity, invalidate translations over the slot window, and publish one image generation per slot | `game/overlay/overlay_slot.*` (one slot), `overlay_images.*` (both slots + loader observer), `level_slot_image.h`, `shared_slot_image.h` (retail module tables) | `installOverlayLoadObserver`, `OverlaySlot::publish` | `docs/issues/0020` |
| Exact title facts | Hold verified executable, memory, HLE, CD, DMA, pad and projection facts as the typed objects `ToyStory2Runtime` hands the framework | `game/core/guest_facts.h`, `game/cd/` | `ts2::facts::kProgramImage`, `kPlatformHlePlan`, `kCdStreamCallbackLayout` | `tools/overlay_map.py` |
| STR completion ring | The FMV wait's ring state words and the two guards that gate the only routine that can satisfy it; each address re-derived from the retail bytes | `game/cd/str_completion_layout.h` | `ts2::cd::kStrCompletionLayout` | `docs/issues/0027` |
| Whole-file load path | Answer the guest's whole-file read (`0x80082608`) from the authenticated disc image and own the bounded retry policy in its caller (`0x80082728`), refusing rather than transferring a request it cannot honour exactly | `game/cd/file_transfer.*` | `ts2::cd::FileTransfer`, `installFileTransferOverride` | `docs/issues/0035` |
| Sound-bank processor | Run the guest's own VAB bank routine (`0x8007F108`) under the framework's bounded resume loop, so its `0x8007F174` size assertion costs bounded display fields and ends as a named refusal instead of an unbounded `VSync` spin | `game/audio/guest_sound_bank.*` | `ts2::audio::installSoundBankProcessorOverride` | `docs/issues/0035` |
| Boot synchronization | Preserve measured graphics state without guest-owned timing | `game/boot/` | `initializeGuestMain`, `installNativeSyncOverrides` | `docs/issues/0025` |
| Level start's first presentation | Present the guest's own level card on a player level, and suppress only the demo-forced `gfx\loading.raw` card, armed per level start from the same `DAT_800A120C` the guest's own guard reads | `game/boot/level_start_presentation.*` (armed from `game/loop/resident_preparation.cpp`) | `ts2::LevelStartPresentation::arm`, `levelStartFirstPresentationEntry` | `docs/re-frontier.md` RE-20, RE-22, RE-23 |
| Pad recording phase | Name which screen is taking input, so a recording's presses are offsets from a screen's entry rather than absolute pad frames | `game/input/toystory2_input_phase.*` | `ts2::InputPhase::of`, `ToyStory2Runtime::inputPhase` | `docs/re-frontier.md` RE-23 |
| Frame orchestration | Own bounded front-end, intro-movie, post-level transition and title-poll calls (one display field per step through `ResumableGuestCall`, and one host slice per native movie frame), plus the bounded asset-load dispatcher and resident sequencing; present the guest's layers and re-present a playing movie on top | `game/loop/` | `createFrameDriver`, `stepOuterLoop` | `docs/issues/0025`, `docs/issues/0038` |
| Native input | Publish measured pad packets and title-visible state | `game/input/` | `installNativePadOverrides` | `docs/issues/0020` |
| Rendering | Publish authored projection, constrain widescreen behavior (the resident widening engages only for the resident leg and is retired by publishing the guest's own display mode again), capture authored camera/visibility/mesh inputs, decode resident mesh commands, name the producer instance of each guest projection (60 fps provenance scopes), and declare when the frame is continuous with the previous one | `game/render/` (`guest_widescreen.*`, `resident_widescreen.*`, `resident_camera_history.*`, `resident_scene_history.*`, `resident_projection_scopes.*`, `resident_temporal_source.*`) | `guestWidescreenPolicy`, `installResidentSceneObservationOverrides`, `render::installResidentProjectionScopes`, `render::ResidentTemporalSource` | `docs/issues/0030-native-scene-producers-are-not-grounded-at-the-p.md`, `docs/issues/0036-toy-story-2-60fps-has-no-re-runnable-world.md` |
| Binary and asset tools | Derive title facts from authenticated game bytes | `tools/` | individual Python CLIs | `CLAUDE.md` |
| Hermetic boundaries | Verify title-owned CD, projection, finite-frame, and image/execution contracts without gameplay | `tests/` | CTest targets | `README.md` |
| Product verification | Supply title targets to the framework's shared configure/build/test and execution-boundary verifier | `tools/verify.py` | `main` | `README.md` |
| Gameplay control tools | Exact-frame pad routes, a recorded phase-keyed replay to replay one, Buzz's guest-RAM position words with their cited instructions, the movement judge, and the control-channel ledger reader | `tools/ts2_route.py`, `tools/headless_run.py`, `tools/ts2_guest_words.py`, `tools/verify_route.py`, `tools/verify_movement.py`, `tools/execution_ledger.py` | `verify_route.py --route/--negative/--determinism`, `verify_movement.py --run/--negative`, `headless_run.py --pad` | `README.md` |
| PSX platform | Own Lightrec, CPU state, memory, invalidation, native dispatch, hardware and presentation | `external/psxport/` | `psx::cpu::dispatchGuest` | `external/psxport/AGENTS.md` |

## Where new work goes

- CPU decoding, lowering, cache, invalidation and executor exits: `external/psxport/runtime/cpu/`.
- Title-specific native behavior or override policy: the smallest cohesive module under `game/`.
- Environment/CLI/settings ingestion: psxport's configuration API or the Python launcher; never a
  title subsystem-local `getenv`.
- Product diagnostics: Lucent at the owning call site.
- Offline evidence extraction: a modular Python tool under `tools/`; never executable guest source.
- A measurement, census, or claim-recording tool has no home here. The gate is the product build, its
  unit tests and the C++ policy checks; anything else belongs in psxport or nowhere.
