# Toy Story 2 — title rules

This repository ports the USA PlayStation release `SLUS_008.93` as one native/Lightrec hybrid over
the shared `external/psxport` framework. Read `../AGENTS.md`, `external/psxport/AGENTS.md`, and the
portfolio native/dynarec contract before changing execution architecture.

## Product contract

- The authenticated executable and streamed modules are runtime data.
- psxport's per-`Core` Lightrec executor runs every guest instruction not deliberately owned by a
  verified native title subsystem.
- Gameplay never links, selects, or falls back to an interpreter. An interpreter is permitted only
  in a separately built diagnostic target.
- No offline translator, generated guest-source corpus, seed manifest, generated dispatcher, or
  executable static substrate belongs in this repository.
- Until psxport's maintained no-interpreter Lightrec fork is integrated, the product must stop at its
  single named executor-unavailable fault. Do not restore an older execution path to get past it.

## Ownership

`ToyStory2Runtime` composes title behavior. `game/core/guest_execution.*` is the one title-side
adapter to psxport's typed guest-call, original-call, and image-scoped native-dispatch APIs. Native
input, finite-frame, graphics synchronization, scene observation, projection, and asset-decoding
owners remain cohesive modules; do not grow a runtime or entry-point monolith.

Native overrides are registered only after the resident image is authenticated and activated.
Overlay identity is image generation plus guest address: LEVEL modules reuse `0x800D12C0`, while
MEMORY and FMV reuse `0x800D5D20`. Loading or replacing one must invalidate translated code and stale
override policy through psxport.

Configuration is ingested at the launcher/configuration boundary and passed as typed state. Product
modules do not read environment variables. Product diagnostics use Lucent; do not add `fprintf`,
`printf`, `std::cerr`, `cfg_log*`, or logger calls wrapped in conditionals.

## Evidence and workflow

Before non-trivial work run:

```sh
uv run --frozen python tools/info.py brief <terms>
uv run --frozen python tools/re_frontier.py next
uv run --frozen python tools/catalog.py search <symptom>
```

Preserve exact-image evidence, oracle scenarios, and native subsystem contracts. Static analysis may
produce symbols and non-executable metadata; it may not emit guest functions. Fix CPU semantics in
psxport's shared Lightrec integration, not with title-address overrides.

The first runtime discriminator must consume exact `SLUS_008.93`, execute nonzero Lightrec blocks,
reach resident and streamed-module code, exercise image-generation invalidation, and agree with an
independent oracle at a bounded checkpoint. Boot, FMV, or one captured frame is insufficient.
Representative gameplay must additionally cover player input, pause/unpause, camera movement,
module replacement, coherent rendering/audio, and sustained frame progression.

## Preserved title facts

- The supported executable is 598,016 bytes; identity is recorded in
  `docs/info/exe-identity.txt`.
- The executable plus 22 loaded code modules form the runtime code corpus. The four-byte LEVEL00
  placeholder is not code.
- LEVEL alternatives reuse `0x800D12C0`; MEMORY and FMV reuse `0x800D5D20`; FMV enters at
  `0x800D6628`.
- The first resident call boundary is `0x80089344`; IRQ resume `0x80088A2C` is a mid-function
  re-entry, not a normal function start.
- Model-table reset `0x80041F38` includes the delay-slot increment at `0x80041FFC`.
- Retail pad buffers are `0x800CF8A0` and `0x800CF8C8`; active-low Cross is `0xBF` and release is
  `0xFF` in the measured packet byte.
- Authored projection leaves are `0x80083CD4` and `0x80083CF4`, with initialization values
  `256/120/160`.
- The finite frame owner must never dispatch guest VSync `0x80088628`.
- `.RAW` payloads use Traveller's Tales' `DecompressRAW` LZ format; the retained corpus check covers
  813 chunks across 46 files with both CRCs.
- True widescreen changes projection, viewport/scissor, and any proven culling owner at the semantic
  producer boundary. It never stretches output or samples adjacent frames.

## Tooling and files

`./run.sh` delegates to locked Python and launches the current target. Other automation is Python,
modular, and DRY. Build outputs live under `build/`; bounded diagnostic artifacts live under stable
`scratch/` children. Game assets are never committed or packaged.

Run `uv run --frozen python tools/check_structure.py` after structural changes. Its selftest must
demonstrate both accepting and rejecting cases for retired static dependencies, direct C/C++ stderr,
stray `getenv`, and the 1,200-line source cap.
