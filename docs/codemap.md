# Codemap

The single-page answer to which subsystem owns each responsibility, where it lives, and where new
responsibility should go. Capability coverage belongs in `docs/project-state.md`, epic intent in
`docs/project-goals.md`, atomic work in `docs/issues/`, and binary-evidence ordering in
`docs/re-frontier.md`.

Toy Story 2 is one standalone title. There is no `titles/` layer, shared game-engine layer, or vendored
decompilation. `external/psxport/` is the framework boundary; title behavior and evidence remain in this
repository.

## Subsystems

| Subsystem | Responsibility | Current / target location | Entry point | Deep doc |
|---|---|---|---|---|
| Process composition | Construct the framework owners, install the title runtime and registry, and enter boot | `game/core/main.cpp` | `main` | `CLAUDE.md` |
| Title runtime | Own framework-facing Toy Story 2 behavior, boot dispatch, override installation, and title capability declaration | `game/core/toystory2_runtime.cpp`, `game/core/toystory2_runtime.h` | `ToyStory2Runtime` | `docs/re-frontier.md` |
| Legacy compatibility seam | Publish measured addresses and bounded compatibility callbacks still consumed through the framework's legacy interface | `game/core/game_config.cpp`, `game/core/game_hooks.cpp`, `game/core/legacy_game_interface.h` | `makeGameConfig` | `docs/re-frontier.md` |
| Recompiled-code registry | Register resident dispatch, module descriptors, and generated override routing | `game/core/recomp_register.cpp` | `register_recompiled_code` | `game/recomp_seeds.json` |
| Projection publication | Bind the two measured libgte setters and preserve their guest effects while publishing authored projection | `game/core/game_config.cpp` | `ts2::legacy::measuredConfig` | `docs/issues/0021-toy-story-2-projection-setters-were-not-recorded.md` |
| Field clock | Deliver one title field turn: pad sample, guest VBlank, SPU advance, and neutral queue commit | `game/sync/field_clock.cpp`, `game/sync/field_clock.h` | `field_turn` | `docs/re-frontier.md` |
| Generated substrate | Hold reproducible recompiler output only; never hand-edit generated translation units | `generated/` | generated dispatch tables | `docs/re-frontier.md` |
| Build graph | Define the product, boundary tests, generated object ownership, and normal verification gates | `CMakeLists.txt`, `cmake/toystory2_port.cmake` | `toystory2_port` | `CLAUDE.md` |
| Product launcher | Resolve the locked environment, provision inputs, configure the isolated player build, and launch the intended product | `run.sh`, `bootstrap.py`, `tools/run.py` | `bootstrap.py` | `docs/project-goals.md` |
| Disc and executable provisioning | Resolve user-supplied media, list/extract files, and verify executable identity | `tools/resolve_disc.py`, `tools/discdump.py`, `tools/extract_exe.py`, `tools/extract_disc_files.py` | `resolve_disc` | `docs/references.md` |
| Static recompilation input | Derive and verify the resident/module emission manifest from measured seeds | `tools/recomp_substrate.py`, `game/recomp_seeds.json` | `recomp_substrate.py` | `docs/re-frontier.md` |
| Overlay and code discovery | Census plain code and derive exact resident overlay placement and entry boundaries | `tools/code_scan.py`, `tools/base_fit.py`, `tools/overlay_map.py` | `overlay_map.py` | `docs/re-frontier.md` |
| Ghidra evidence supply | Construct the identity-checked RAM image and cross-reference guest addresses | `tools/ram_image.py`, `tools/ghidra_xref.py`, `tools/re_xref.py` | `re_xref.py` | `docs/references.md` |
| Asset decoding | Frame and decode Traveller's Tales `.RAW` containers through one chunk walker | `tools/raw_probe.py`, `tools/raw_unpack.py` | `raw_unpack.py` | `docs/references.md` |
| Binary boundary verifiers | Check crt0, CD commands, pad buffers, projection publication, frame fences, and model reset against shipping code | `tools/verify_crt0.py`, `tools/verify_cd_command.py`, `tools/verify_pad_buffers.py`, `tools/verify_projection_publication.py`, `tools/verify_frame_fence.py`, `tools/verify_model_table_reset.py` | each tool's `main` | `docs/re-frontier.md` |
| CPU-oracle boundary | Compare shipping generated execution with the independent CPU oracle | `tools/compare_recomp_boundary.py`, `tests/toystory2_recomp_boundary.cpp` | `toystory2_recomp_boundary` | `docs/re-frontier.md` |
| Hermetic title boundaries | Exercise title-owned runtime and projection seams without reproducing production logic | `tests/toystory2_projection_boundary.cpp` | `toystory2_projection_boundary` | `docs/project-state.md` |
| Project information | Query durable claims, trusted instruments, atomic issues, capability state, goals, and placement | `tools/info.py`, `tools/catalog.py`, `tools/re_frontier.py`, `docs/info/`, `docs/issues/` | `info.py brief` | `docs/project-state.md` |
| Public-history audit | Refuse restricted assets and machine-local paths before publication | `tools/go_public.py` | `go_public.py` | `CLAUDE.md` |
| PSX platform | Own recompiler mechanics, guest hardware, generic rendering/presentation, SDK HLE, UI, and shared harnesses | `external/psxport/` | framework `Game` / `Core` | `external/psxport/CLAUDE.md` |

## Where new work goes

- New title behavior or a typed framework-facing title policy goes beside
  `game/core/toystory2_runtime.cpp`; do not add it to the process entry point.
- One-field Toy Story 2 timing or callback ordering goes under `game/sync/`; reusable platform timing
  belongs in psxport.
- Newly measured immutable guest addresses enter `game/core/game_config.cpp` only after their verifier
  and RE-frontier dependency land together.
- Native camera, scene, actor, effect, and 2D producers each get cohesive title-owned modules under
  `game/`; they do not grow `main.cpp`, `game_config.cpp`, or generated code.
- Game-independent MDEC, GPU, presentation, input, audio, or SDK behavior belongs in psxport, with a
  framework regression before a consumer pin bump.
- Reusable binary/asset analysis belongs in framework tooling. A title-specific proof stays in
  `tools/` and records its claim or instrument under `docs/info/`.
- Atomic symptoms, investigations, blockers, findings, and resolved root causes go in `docs/issues/`.
  Capability state does not go there.
