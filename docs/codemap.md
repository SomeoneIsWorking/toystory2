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
        ts2::stepResidentFrame -> ts2::stepOuterLoop -> guest calls through psx::cpu::ResumableGuestCall
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
| `ts2` | `GuestMainBoot` | Run the measured initialization prefix of guest main `0x8007A9E8` (`initialize`) and then its overlay initialization (`finishOverlayInitialization`), stopping before the non-returning guest outer loop, which `ts2::stepOuterLoop` owns. |
| `ts2` | `GraphicsSync` | `install` the graphics-init, resident-graphics-init, graphics-shutdown and field-barrier replacements (`0x8003A218`, `0x80039D9C`, `0x8003A838`, `0x8003FA68`): the same guest state transitions with no guest-owned VSync. |
| `ts2` | `LevelStartPresentation` | Arm or retire the `0x8007C278` override per level start from the demo flag the guest's own guard reads, and suppress only the demo-forced LOADING card. |
| `ts2` | `LevelStartPresentation::demoGuardForcesLoadingCard`, `levelStartPresentationAddress`, `levelStartFirstPresentationEntry` | The demo-guard decision, its guest address, and the override entry point. |

### `game/frame/` — the frame turn

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `createFrameDriver` / `ToyStory2FrameDriver` | The `FrameDriver` the runtime installs; runs `stepResidentFrame` per frame over the frame state below. |
| `ts2` | `FrameCallState` | What a frame turn must carry from one step to the next: the outer-loop phase, the resident preparation, the one resumable field call (`fieldCall()`, bound to the Core on first use), the intro-movie step and the running overlay call. |
| `ts2` | `CoreFrameBoundary` | The shipping implementation of both boundaries over one `Core`: every guest RAM address the front end, the overlays and the resident leg use, and every call it makes through `ts2::ResumableGuestCall`. |
| `ts2` | `SelectionCall` | Which guest call the one resumable field call is currently running. |
| `ts2` | `ResidentFrameBoundary` | The finite operations at the resident main-loop boundary, in order: quota, logic frame, input, display fields, deferred display, present, update, audio. The deferred display draws the OT the previous update built, so the record is sealed before this update writes the next one. |
| `ts2` | `FrameCut` | Whether the record an update built is a cut: another level (`0x800A16A8`) or area object table (`0x800A1274`), the camera placed afresh (`0x80065CD0`), or the camera update (`0x80068E2C`) switching source with no blend pending (`0x800A1498 < 1`). `install` observes both camera routines; `notePassEnded` runs after each update; `ToyStory2Runtime::sealedFrameIsCut` reads `isCut`. |
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
| `ts2::frame` | `FieldCall` | The title's policy over `psx::cpu::ResumableGuestCall` for a call the frame turn drives across display fields: publish the call's arguments, count and log the display fields it waited on, and treat a turn that ended `FrameBoundary` (a field) or `CooperativeYield` (a native replacement's slice) as the end of a host step while a `BudgetExhausted` turn continues inside the same step. |
| `ts2` | `GuestCall`, `kOneFieldCallTurns`, `kFiniteInitializationSliceLimit`, `kResidentUpdateSliceLimit`, `publishCallArguments`, `callGuestToReturn`, `callFiniteGuestToReturn`, `guestString` | This title's guest-call facts: the entry, the return address, the arguments, the measured slice caps, one display field's requirement for a leaf call, and the one reader for a NUL-terminated string out of guest RAM. |
| `ts2` | `callOriginalToReturn`, `callOriginalToReturnResuming` | Run a guest body that a native override replaced, once, or resumed across bounded turns. |
| `ts2` | `guestString` | The one reader for a NUL-terminated string out of guest RAM, bounded by the longest name this title's guest spells. |
| `ts2` | `installResidentOverride` | Register one image-scoped native override, refusing when the resident image identity is unavailable. |
| `ts2` | `kFiniteInitializationSliceLimit`, `kResidentUpdateSliceLimit` | The slice bounds an asset decode and a resident update may take before they fail closed. |

### `game/facts/` — measured title facts

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::facts` | `kProgramImage`, `kPlatformHlePlan`, `kPadBufferLayout`, `kCdStreamCallbackLayout`, plus the crt0, overlay-slot, projection and pad constants | The verified `SLUS_008.93` facts the runtime hands the framework, each citing the bytes it came from. An unmeasured fact is absent deliberately; a plausible-looking wrong address does not fail cleanly. |

### `game/runtime/` — composition

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `ToyStory2Runtime` | The `GameRuntime` the framework runs: every measured fact group, the render capabilities (the record path with temporal interpolation), the packet pool windows, the cut declaration, the widescreen policy, the frame driver, the input phase, and the registration of every resident override. |
| `ts2` | `ToyStory2Context` | Everything one `Core` owns, in the order a run reaches it: the boot owners, the code images and sound banks, the pad and the movie player, the frame cut, the actor incarnations and the actor being drawn. `ToyStory2Runtime::registerOverrides` calls each owner's `install` here. |
| `ts2` | `context(Core&)` | The one accessor for that per-`Core` state; refuses a Core with no context. |

### `game/input/` — host input

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `PadOwner` | The native pad owner: `service` publishes the host packet into the retail slot buffers (`0x800CF8A0`, `0x800CF8C8`) once per frame, `decode` answers the guest's decode from that buffer, `initialize`/`shutdown` are the ends of the guest's pad lifetime, and `install` registers the three overrides. |
| `ts2` | `InputPhase` | The phase a pad recording is keyed on: four guest words that hold still while the screen that owns them is up, packed into the key `GameRuntime::inputPhase` returns. |

### `game/cd/` — loading and CD facts

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::cd` | `FileTransfer` | Answer the guest's whole-file read (`0x80082608`) from the authenticated disc image, or refuse it with a reason and transfer nothing; `FileTransfer::install` registers it and the bounded retry policy in its caller (`0x80082728`). |
| `ts2::cd` | `StockLibcdLayout`, `kStockLibcdLayout` | The identity-checked stock-libcd entry points and state the title configuration and its boundary test consume. |
| `ts2::cd` | `StrCompletionLayout`, `kStrCompletionLayout`, `kFmvWaitEntry`, `kFmvWaitRetries` | The STR ring state the FMV player blocks on, and the FMV overlay's own bounded wait. |

### `game/overlay/` — streamed code images

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `OverlaySlot` | The code-image owner of one fixed guest-RAM slot: authenticate the disc source and the transferred bytes, publish one image identity, retire with translated-code invalidation over the slot window. |
| `ts2` | `OverlayModule` | One retail file the loader places in a slot: guest spelling, disc location, exact byte count and SHA-256. |
| `ts2` | `OverlayImages` | Both slots (LEVEL `0x800D12C0`, shared MEMORY/FMV `0x800D5D20`), the slot a load destination fills, and `installLoadObserver`: observe the retail file loader (`0x80082508`), run the original, then authenticate, publish or retire the slot it filled. |
| `ts2` | `LevelSlotImage`, `SharedSlotImage` | The retail module tables for the two slots. |

### `game/audio/` — sound banks

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::audio` | `SoundBankProcessor` | `install` the guest's own VAB bank routine (`0x8007F108`) to run through the framework's bounded resume loop, so its size assertion costs bounded display fields and ends as a named refusal. |

### `game/fmv/` — movies

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2::fmv` | `GuestMoviePlayer` | `install` the replacement for the FMV overlay's own streaming player (`0x800D7088`), scoped to the FMV image generation: one movie frame per host turn, and exactly the retail return value (0 at end, the cold-start word on a skip). |

### `game/widescreen/` — 16:9

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `guestWidescreenPolicy` | The title's answer to "is this picture wider than the console's 4:3?", from the configured aspect. |
| `ts2` | `ResidentWidescreenCull` | On the record path the guest keeps its retail 512-wide draw and centre and psxport's canvas adds the margins; this owner widens only what the guest culls against: the visibility window (`0x80027AF0`) and the published screen rectangle (`0x80010000`), while the latched plan is widescreen. |

### `game/render/` — render producers

| Namespace | Owner | Responsibility |
|---|---|---|
| `ts2` | `SlotMeshProducers` | Key the faces of the static mesh submitter (`0x800100E4`) and the rigid mesh drawer (`0x80017FF8`): each packet in the slot array is `(submitter, instance slot table *0x800A11CC, face entry)`. One object per call, `(submitter, instance slot table, 0)`; `save` keeps the call registers, the GTE control and the bytes of every range the body read (`MemoryRange`), `render(t)` re-runs the native body over host copies on blended control. Native bodies: `rigid_mesh_drawer.*` (`0x80017FF8`), `static_mesh_drawer.*` + `static_mesh_plan.cpp` (`0x800100E4`, positive-header meshes), the four subdividers (`quad_split*`, `tri_split*` over `split_engine.*`), `slot_packets.*` (links a packet and binds it to the open key), `slot_release.*` (child release), `mesh_culling.*`, all on `mesh_cpu.h` (register-level body, scratchpad `0x1F800000`). `MeshDrawers::plan` trials the call on host memory; a call it cannot run natively (negative-header meshes, unported sites) runs on the guest and `keyFaces` keys its slot-array packets as before. `slotEntries` counts a mesh's entries per submitter. |
| `ts2` | `ResidentMeshLayout`, `ResidentMeshCommand`, `decodeResidentMeshLayout`, `decodeResidentMeshCommand` | The checked source layout and command walk of one resident mesh. |
| `ts2` | `ActorIncarnations` | Which life of a pooled actor an object key names: observes the guest's actor reset `0x8006A9FC` (level start and respawn) and bumps that record's generation, folded above the RAM offset by `incarnationObject`. |
| `ts2` | `ActorProducers` | Overrides the actor renderers `0x8002518C` and `0x80024174` to note the drawn actor's incarnation (`drawing()`) around the original body; `partObject` names a part as `(a0 part record, incarnation)`, so each part is one object. Opens no scope itself, so unported drawers stay unkeyed. Installs `PartStateRender` for `0x8002518C`. |
| `ts2` | `PartFaceDrawers` | Native ports of the part drawers `0x8001C920` (prelit) and `0x8001DFE4` (per-corner normals) over `EmitMemory`: walk the part's faces and write each GT3/GT4 packet inside `(0x8002518C, part object, 0)`; the override and `PartStateRender` run the same body. Packets are byte-identical to the guest's (override differential: 0 mismatches on both). The DCPL-lit drawer `0x8001CD34` stays on the guest: no replay reaches it, so parts drawn with it keep their packets as drawn. |
| `ts2` | `PartDrawRecorder`, `PartStateRender` | `save` keeps a part draw's inputs (drawer and arguments, GTE control registers, packet cursor, OT base and the guest ranges the body reads); `render(t)` blends the control registers of the same draw from the two records and re-runs `PartFaceDrawers::run` over host memory, emitting the host-built OT through `emitHostOrderingTable`. |
| `ts2` | `OrderingTables` | Names the two graphics buffers' OTs (`0x2B4` into the buffer, `0x57C` buckets) so entries carry their bucket. |

### `tools/`, `tests/`, `cmake/`

| Path | Responsibility |
|---|---|
| `tools/` | Modular Python owners: the launcher (`run.py`, `psxport_fetch.py`), the gameplay control (`headless_run.py` with `resolve_disc.py`, `execution_ledger.py` and `ts2_route.py`), the gate (`verify.py`), the RE evidence path (`ram_image.py`, `ghidra_xref.py`, `re_xref.py`, `extract_exe.py`, `discdump.py`) and the RE tracker shim (`re_frontier.py`). |
| `tests/` | The hermetic C++ boundaries: `toystory2_projection_boundary` (projection publication), `toystory2_cd_hle_boundary` (stock libcd), `frame_turn_boundary.cpp` (the per-field order, the outer-loop sequencing, the runtime factories, the record path and its widescreen plan), `resident_producers_boundary.cpp` (the pad owner, the slot-mesh and actor-face keys, actor incarnations, the frame cut), `toystory2_execution_boundary` (title execution) and `toystory2_level_start_card_boundary`. They exercise the shipping owners through a seam and never reimplement them. |
| `cmake/toystory2_port.cmake`, `CMakeLists.txt` | The title source list, the include root (`game`), and the CTest surface. |

## Who owns it

### The frame turn

- psxport's native frame loop owns the turn and asks `ts2::ToyStory2Runtime::createFrameDriver` for the driver.
- `ts2::ToyStory2FrameDriver::stepFrame` builds `ts2::CoreFrameBoundary` over the driver's `ts2::FrameCallState` and calls `ts2::stepResidentFrame`.
- `ts2::stepResidentFrame` runs the measured order through `ResidentFrameBoundary`: `displayFieldQuota`, `beginLogicFrame`, `sampleInput`, `tickDisplayField` × quota, `serviceDeferredDisplay`, `present`, `updateResidentGame` (then `FrameCut::notePassEnded`), `advanceAudio`.
- `updateResidentGame` is `ts2::stepOuterLoop(OuterLoopState&, OuterLoopBoundary&)`, which performs ONE finite title operation per call and records the next `OuterLoopPhase`.
- A guest call that must return goes through `ts2::callGuestToReturn` / `ts2::callFiniteGuestToReturn`.
- **While a movie or a loading call blocks**: the driver's `FrameCallState::fieldCall()` holds one `ts2::frame::FieldCall`, over `psx::cpu::ResumableGuestCall`. The guest's own field barrier (`0x8003FA68`, owned by `GraphicsSync::install`) publishes the elapsed fields and then exits the executor with `FrameBoundary`, so the turn comes back to `ResumableGuestCall::advance` as `Progress::fieldBoundary`; the native movie player (`game/fmv`) exits with `CooperativeYield` instead, which is `Progress::hostSlice`. Either way `stepOuterLoop` returns, the host presents that field, and the call is resumed at the same guest PC on the next step. Host input is pumped once per turn by `sampleInput`, so a blocking movie never stops the pad, the control channel or the window's events.

### Host input → guest pad buffer

- psxport's `psx::input::HostInput` is the ONE owner of host input: it drains the SDL event queue, holds the
  delivered key state and the open controllers, and latches the P / `.` debug edges. `Pad` consumes its
  mask through `Pad::pollHostInput`, the one pump every site calls.
- The pump is serviced once per frame by `ResidentFrameBoundary::sampleInput` → `core.game->pad.serviceFrame()`.
- The same step calls `ts2::PadOwner::service(Core&)`, which fills the retail slot buffers `0x800CF8A0` / `0x800CF8C8` through `Pad::fillBuffer`.
- The guest reads that buffer through its own `0x8003AC58`, which `PadOwner::install` replaces with `PadOwner::decode` (active-low, release `0xFF`).
- **Movie skip**: a Start press travels the ordinary pad path above; psxport's native FMV owner resolves it and the title's `GuestMoviePlayer` override returns the cold-start word `0x800A1670`, which ends the remaining intro movies.
- **The debug control channel** (loopback, always open) drives host input through psxport's own pad path, so a channel-injected press is the same press a player makes.
- **Replay phase**: `ts2::InputPhase::of(Core&)` packs four still guest words into the key `ToyStory2Runtime::inputPhase` returns, so a recording's presses are offsets from the screen that owns them.

### Guest draw → presentation

- `ToyStory2Runtime::renderCapabilities` selects psxport's record path: the device's executed GP0 is the picture, sealed once per logic frame and replayed (`psxport/docs/presentation.md`).
- The guest builds update N's OT; the deferred field service (`0x80021028` → `0x8003A888`) draws it during frame N+1's `serviceDeferredDisplay`, then swaps the buffers. `ResidentFrameBoundary::present` seals that record (`commit(core, guestFields, nullptr)`) before update N+1 runs, then resets the packet spans.
- **60 fps in-between**: psxport's `FramePresenter` blends the two shown records by key. Keys come from `SlotMeshProducers`; anything unkeyed is drawn as the newer record. A record `FrameCut` declares a cut is shown without an in-between.
- **Widescreen**: psxport's record display plan widens the canvas around the retail picture; `ResidentWidescreenCull` widens the guest's culling so the margins are filled. Front-end screens show their authored width.

### CD and streaming

- The guest's own file loader (`0x80082508`) runs through `OverlayImages::installLoadObserver`, which authenticates the disc source and the transferred bytes, publishes or retires the slot identity, and installs the FMV player when the shared slot published FMV.
- A whole-file read is answered by `ts2::cd::FileTransfer::transfer` from the authenticated disc image, with the retry policy in its caller bounded.
- Asset decodes (`0x8003D88C` → `0x80021190`) run inside `callFiniteGuestToReturn` on the initialization bound; a sound-bank processor that reaches its assertion is bounded by `callOriginalToReturnResuming`.

### Audio

- psxport's `SpuAudio::frame()` is advanced once per turn by `ResidentFrameBoundary::advanceAudio`.
- Movie audio is psxport's native FMV owner's own XA stream; no title owner writes samples.

### The debug control channel

- psxport owns the channel (`Game`'s debug server, loopback, port movable by env). The title contributes no route of its own: input, frames and the ledger are read through psxport's existing owners, and `tools/headless_run.py` reads the run-end ledger from the log.
