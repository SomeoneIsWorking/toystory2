# Codemap

Toy Story 2 owns title identity, native behavior and enhancement policy. `external/psxport` owns PSX
hardware and the dynarec runtime. Dependencies point from the title into psxport; psxport never names
Toy Story 2.

```text
run.sh -> bootstrap.py -> tools/run.py -> CMake product
                                         |
                                         v
game/main.cpp -> ts2::TitleSession -> psxport_install_game(ts2::ToyStory2Runtime)
                                       |
                                       v
                    psxport frame loop -> FrameDriver::stepFrame (ts2::ToyStory2FrameDriver)
                                       |
                                       v
        ts2::stepResidentFrame -> ts2::stepOuterLoop -> guest calls through ts2::ResumableGuestCall
```

## Directories

Every directory below is one subsystem: its namespace, its classes, and what each owns. A behaviour
that is not in this table does not belong to this title.

### `game/` — process entry

| Namespace | Class | Responsibility |
|---|---|---|
| global | `main` | Parse `--help` and the optional executable path, construct one `TitleSession`, return its exit code. Names no subsystem. |

### `game/boot/` — boot and shutdown

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `TitleSession` | One boot-to-exit run: self-provision the executable from the disc, bring up the peripherals in binding order, register overrides, enter the native boot, and tear everything down by destruction. |
| `ts2` | `kDefaultExe`, `kDiscExePath` | The executable name SYSTEM.CNF boots and the disc path it is extracted from. |
| `ts2` | `initializeGuestMain`, `finishGuestMainBoot` | Run the measured initialization prefix of guest main `0x8007A9E8` and then its overlay initialization, and stop before the non-returning guest outer loop, which `ts2::stepOuterLoop` owns. |
| `ts2` | `installNativeSyncOverrides` | Install the graphics-init, resident-graphics-init, graphics-shutdown and field-barrier replacements (`0x8003A218`, `0x80039D9C`, `0x8003A838`, `0x8003FA68`): the same guest state transitions with no guest-owned VSync. |
| `ts2` | `LevelStartPresentation` | Arm or retire the `0x8007C278` override per level start from the demo flag the guest's own guard reads, and suppress only the demo-forced LOADING card. |
| `ts2` | `demoGuardForcesLoadingCard`, `levelStartPresentationAddress`, `levelStartFirstPresentationEntry` | The demo-guard decision, its guest address, and the override entry point. |

### `game/frame/` — the frame turn

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `createFrameDriver` / `ToyStory2FrameDriver` | The `FrameDriver` the runtime installs; runs `stepResidentFrame` per frame over the frame state below. |
| `ts2` | `FrameCallState` | What a frame turn must carry from one step to the next: the outer-loop phase, the resident preparation, the one resumable field call (`fieldCall()`, bound to the Core on first use), the intro-movie step and the running overlay call. |
| `ts2` | `CoreFrameBoundary` | The shipping implementation of both boundaries over one `Core`: every guest RAM address the front end, the overlays and the resident leg use, and every call it makes through `ts2::ResumableGuestCall`. |
| `ts2` | `SelectionCall` | Which guest call the one resumable field call is currently running. |
| `ts2` | `ResidentFrameBoundary` | The finite operations at the resident main-loop boundary, in measured order: quota, logic frame, input, display fields, deferred display, update, audio, present. |
| `ts2` | `stepResidentFrame` | The measured per-frame sequencing function. |
| `ts2` | `OuterLoopState`, `OuterLoopPhase`, `PostResidentTransition`, `SelectionProgress`, `ResidentPreparationProgress` | Which leg of the guest's main loop the host is presenting, and how each step ended. |
| `ts2` | `OuterLoopBoundary` | The guest operations `stepOuterLoop` needs, one phase at a time. |
| `ts2` | `stepOuterLoop` | One finite title operation per call: a front-end poll field, a selection iteration, a resident update, a post-level transition field. |
| `ts2` | `ResidentPreparation` | Run the guest's own level start `0x8007BEC4` and its play-loop entry block as two resumable calls, so every word of the entry state is written by the code that owns it. |

### `game/execution/` — the guest-call seam

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `GuestCall`, `NativeGuestFunction` | One typed guest call: address, return sentinel, arguments, optional stack argument, and the owner name that appears in every refusal. |
| `ts2` | `callGuestToReturn`, `callFiniteGuestToReturn`, `executeFiniteGuestCall` | Bounded guest calls that must return: one turn, or a bounded number of resumed turns for a finite initialization transaction. |
| `ts2` | `ResumableGuestCall` | One guest call that spans display fields or host slices: `begin`, `advance` to the next field boundary / host slice / return, `result`, `abandon`. |
| `ts2` | `callOriginalToReturn`, `callOriginalToReturnResuming` | Run a guest body that a native override replaced, once, or resumed across bounded turns. |
| `ts2` | `installResidentOverride` | Register one image-scoped native override, refusing when the resident image identity is unavailable. |
| `ts2` | `kFiniteInitializationSliceLimit`, `kResidentUpdateSliceLimit` | The slice bounds an asset decode and a resident update may take before they fail closed. |

### `game/facts/` — measured title facts

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::facts` | `kProgramImage`, `kPlatformHlePlan`, `kPadBufferLayout`, `kCdStreamCallbackLayout`, plus the crt0, overlay-slot, projection and pad constants | The verified `SLUS_008.93` facts the runtime hands the framework, each citing the bytes it came from. An unmeasured fact is absent deliberately; a plausible-looking wrong address does not fail cleanly. |

### `game/runtime/` — composition

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `ToyStory2Runtime` | The `GameRuntime` the framework runs: every measured fact group, the render capabilities, the fps60 presenter, the widescreen policy, the frame driver, the input phase, and the registration of every resident override. |
| `ts2` | `ToyStory2Context` | The per-`Core` title state: overlay images, camera and scene histories, projection scopes, the resident widening, and the level-start presentation. |
| `ts2` | `context(Core&)` | The one accessor for that per-`Core` state; refuses a Core with no context. |

### `game/input/` — host input

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `initializeNativePad`, `shutdownNativePad`, `decodeNativeDigitalPad`, `serviceNativePad` | The native pad owner: publish the host packet into the retail slot buffers (`0x800CF8A0`, `0x800CF8C8`) once per frame, and answer the guest's decode from that buffer. |
| `ts2` | `installNativePadOverrides` | Install the pad-init, pad-shutdown and digital-pad-decode overrides. |
| `ts2` | `InputPhase` | The phase a pad recording is keyed on: four guest words that hold still while the screen that owns them is up, packed into the key `GameRuntime::inputPhase` returns. |

### `game/cd/` — loading and CD facts

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::cd` | `FileTransfer` | Answer the guest's whole-file read (`0x80082608`) from the authenticated disc image, or refuse it with a reason and transfer nothing. |
| `ts2::cd` | `installFileTransferOverride` | Install the whole-file read and the bounded retry policy in its caller (`0x80082728`). |
| `ts2::cd` | `StockLibcdLayout`, `kStockLibcdLayout` | The identity-checked stock-libcd entry points and state the title configuration and its boundary test consume. |
| `ts2::cd` | `StrCompletionLayout`, `kStrCompletionLayout`, `kFmvWaitEntry`, `kFmvWaitRetries` | The STR ring state the FMV player blocks on, and the FMV overlay's own bounded wait. |

### `game/overlay/` — streamed code images

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `OverlaySlot` | The code-image owner of one fixed guest-RAM slot: authenticate the disc source and the transferred bytes, publish one image identity, retire with translated-code invalidation over the slot window. |
| `ts2` | `OverlayModule` | One retail file the loader places in a slot: guest spelling, disc location, exact byte count and SHA-256. |
| `ts2` | `OverlayImages` | Both slots (LEVEL `0x800D12C0`, shared MEMORY/FMV `0x800D5D20`) and the slot a load destination fills. |
| `ts2` | `installOverlayLoadObserver` | Observe the retail file loader (`0x80082508`), run the original, then authenticate, publish or retire the slot it filled. |
| `ts2` | `LevelSlotImage`, `SharedSlotImage` | The retail module tables for the two slots. |

### `game/audio/` — sound banks

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::audio` | `installSoundBankProcessorOverride` | Run the guest's own VAB bank routine (`0x8007F108`) through the framework's bounded resume loop, so its size assertion costs bounded display fields and ends as a named refusal. |

### `game/fmv/` — movies

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::fmv` | `installGuestMoviePlayer` | Replace the FMV overlay's own streaming player (`0x800D7088`) with psxport's native player, scoped to the FMV image generation: one movie frame per host turn, and exactly the retail return value (0 at end, the cold-start word on a skip). |

### `game/widescreen/` — 16:9

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `guestWidescreenPolicy` | The title's answer to "is this picture wider than the console's 4:3?", from the configured aspect. |
| `ts2` | `ResidentWidescreenProjection` | The title-owned widening of the resident frame canvas: follow the guest's display mode, widen the canvas at the guest's own DRAWENV publication, re-centre the projection per field, widen the visibility window and the published screen rectangle, and present the widened window. |
| `ts2` | `ResidentFrameCanvas` | The measured resident canvas geometry (512x240 at VRAM row 256, retail OFY 120). |

### `game/render/` — render producers

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `ResidentSceneHistory`, `ResidentSceneFrame` | Capture the guest's own visibility batches (`0x8002622C`) and mesh submissions (`0x800100E4`) per frame, decoded, as the input a future native producer would read. |
| `ts2` | `installResidentSceneObservationOverrides` | Install the observation wrappers that record those arguments without changing guest state. |
| `ts2` | `ResidentMeshLayout`, `ResidentMeshVertex`, `ResidentMeshCommand`, `ResidentMeshPrimitive`, `ResidentMeshCommandSummary`, `ResidentMeshMaterialState`, `ResidentMeshDescriptorSample`, `ResidentMeshMaterialCensus` | The checked source layout and command walk of one resident mesh. |
| `ts2` | `decodeResidentMeshLayout`, `decodeResidentMeshVertex`, `decodeResidentMeshCommand`, `decodeResidentMeshPrimitive`, `summarizeResidentMeshCommands` | Those decoders. |
| `ts2::render` | `readResidentView` | The camera the GUEST publishes at the GTE addresses (`0x1F800384`, `0x1F800394`), as the view a time between two fields is built from. |

### `game/fps60/` — the 60 fps in-between

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::render` | `ResidentTemporalSource` | Declare whether the frame about to be presented continues the previous one; a cut or a level start publishes a real frame. |
| `ts2::render` | `ResidentProjectionScopes` | Which guest call is which producer instance, keyed by what the guest itself uses to tell its instances apart (slot-table pointer, first argument, or occurrence in the field). |
| `ts2::render` | `installResidentProjectionScopes` | Install the scope-opening observers for the modelled producers. |
| `ts2` | `ResidentCameraHistory`, `ResidentCameraSample`, `InterpolatedResidentCamera` | The authored camera the guest publishes, its previous/current pair, whether the field continues, and the interpolation between them. |

### `tools/`, `tests/`, `cmake/`

| Path | Responsibility |
|---|---|
| `tools/` | Modular Python owners: the launcher (`run.py`, `psxport_fetch.py`), the gameplay controls (`headless_run.py`, `ts2_route.py`, `verify_route.py`, `verify_movement.py`, `ts2_guest_words.py`, `execution_ledger.py`) and the binary/asset evidence extractors (`extract_exe.py`, `overlay_map.py`, `ghidra_xref.py`, `re_xref.py`, `ram_image.py`, `raw_probe.py`, `raw_unpack.py`, `discdump.py`, `resolve_disc.py`, `re_frontier.py`). |
| `tests/` | The hermetic C++ boundaries: projection publication, stock libcd, the frame driver, title execution, the level-start card. They exercise the shipping owners through a seam and never reimplement them. |
| `cmake/toystory2_port.cmake`, `CMakeLists.txt` | The title source list, the include root (`game`), and the CTest surface. |

## Who owns it

### The frame turn

- psxport's native frame loop owns the turn and asks `ts2::ToyStory2Runtime::createFrameDriver` for the driver.
- `ts2::ToyStory2FrameDriver::stepFrame` builds `ts2::CoreFrameBoundary` over the driver's `ts2::FrameCallState` and calls `ts2::stepResidentFrame`.
- `ts2::stepResidentFrame` runs the measured order through `ResidentFrameBoundary`: `displayFieldQuota`, `beginLogicFrame`, `sampleInput`, `tickDisplayField` × quota, `serviceDeferredDisplay`, `updateResidentGame`, `advanceAudio`, `present`.
- `updateResidentGame` is `ts2::stepOuterLoop(OuterLoopState&, OuterLoopBoundary&)`, which performs ONE finite title operation per call and records the next `OuterLoopPhase`.
- A guest call that must return goes through `ts2::callGuestToReturn` / `ts2::callFiniteGuestToReturn`.
- **While a movie or a loading call blocks**: the driver's `FrameCallState::fieldCall()` holds one `ts2::ResumableGuestCall`. The guest's own field barrier (`0x8003FA68`, owned by `installNativeSyncOverrides`) publishes the elapsed fields and then exits the executor with `FrameBoundary`, so the turn comes back to `ResumableGuestCall::advance` as `Progress::fieldBoundary`; the native movie player (`game/fmv`) exits with `CooperativeYield` instead, which is `Progress::hostSlice`. Either way `stepOuterLoop` returns, the host presents that field, and the call is resumed at the same guest PC on the next step. Host input is pumped once per turn by `sampleInput`, so a blocking movie never stops the pad, the control channel or the window's events.

### Host input → guest pad buffer

- psxport's `Pad` reads the host keyboard/controller and is serviced once per frame by `ResidentFrameBoundary::sampleInput` → `core.game->pad.serviceFrame()`.
- The same step calls `ts2::serviceNativePad(Core&)`, which fills the retail slot buffers `0x800CF8A0` / `0x800CF8C8` through `Pad::fillBuffer`.
- The guest reads that buffer through its own `0x8003AC58`, which `installNativePadOverrides` replaces with `ts2::decodeNativeDigitalPad` (active-low, release `0xFF`).
- **Movie skip**: a Start press travels the ordinary pad path above; psxport's native FMV owner resolves it and the title's `moviePlayerOverride` returns the cold-start word `0x800A1670`, which ends the remaining intro movies.
- **The debug control channel** (loopback, always open) drives host input through psxport's own pad path, so a channel-injected press is the same press a player makes.
- **Replay phase**: `ts2::InputPhase::of(Core&)` packs four still guest words into the key `ToyStory2Runtime::inputPhase` returns, so a recording's presses are offsets from the screen that owns them.

### Guest draw → presentation

- The guest's renderer writes its ordering table and packets; psxport's GTE path rasterizes the captured GP0 stream.
- **Real field**: `ResidentFrameBoundary::present` → `Game::presentation::commit(core, guestFields, temporal)`.
- **60 fps in-between**: the same `commit` hands the frame to `ts2::render::ResidentTemporalSource` (created in `ToyStory2Runtime::createTemporalFramePresentation`), which pairs vertices through the scopes opened by `installResidentProjectionScopes` and only pairs a frame `ResidentCameraHistory::continuous` accepts.
- **Widescreen**: `ResidentFrameBoundary::tickDisplayField` calls `ResidentWidescreenProjection::syncToGuestDisplay` + `beginField` before the guest transforms this field's vertices; `present` calls `presentField` after the update and before rasterization, so the captured stream is rasterized into and presented from the same canvas. Every one of those is a no-op unless `ResidentWidescreenProjection::active`.
- **Front-end screens** (title, level map, movies) present at their authored width: `syncToGuestDisplay` is told the host is not on the resident leg, and the plan is retired.

### CD and streaming

- The guest's own file loader (`0x80082508`) runs through `installOverlayLoadObserver`, which authenticates the disc source and the transferred bytes, publishes or retires the slot identity, and installs the FMV player when the shared slot published FMV.
- A whole-file read is answered by `ts2::cd::FileTransfer::transfer` from the authenticated disc image, with the retry policy in its caller bounded.
- Asset decodes (`0x8003D88C` → `0x80021190`) run inside `callFiniteGuestToReturn` on the initialization bound; a sound-bank processor that reaches its assertion is bounded by `callOriginalToReturnResuming`.

### Audio

- psxport's `SpuAudio::frame()` is advanced once per turn by `ResidentFrameBoundary::advanceAudio`.
- Movie audio is psxport's native FMV owner's own XA stream; no title owner writes samples.

### The debug control channel

- psxport owns the channel (`Game`'s debug server, loopback, port movable by env). The title contributes no route of its own: input, frames and the ledger are read through psxport's existing owners, and `tools/execution_ledger.py` reads the run-end ledger from the log.
