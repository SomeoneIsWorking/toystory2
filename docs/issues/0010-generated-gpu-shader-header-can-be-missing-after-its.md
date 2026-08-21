---
id: 10
title: Generated GPU shader header can be missing after its dependency target completes
status: resolved
symptom: A clean Toy Story 2 rebuild against psxport ce2c83ad reported gen_gpu_shaders built, then Clang failed gpu_vk.cpp because gpu_vk_shaders.h did not exist.
tags: build,framework,cmake,shaders,race
created: 2026-08-21
updated: 2026-08-21
---

## Observation

After reconfiguring Toy Story 2 against exact psxport
`ce2c83adb0fce89c44eb764f2abf3e4f999d32a8`, the first
`cmake --build build --target toystory2_port toystory2_recomp_boundary -j16` reached framework C++
compilation and failed:

```text
runtime/recomp/gpu_vk.cpp:32:10: fatal error: 'gpu_vk_shaders.h' file not found
[ 62%] Building CXX object .../gpu_vk.cpp.o
```

At inspection time `build/psxport_gpu_shaders.stamp` existed. The framework CMake already declared
the header as a `BYPRODUCT`, made `psxport` depend on `gen_gpu_shaders`, and gave that target an
existence-guard generator invocation. The ignored source-tree header appeared afterward; rerunning
the same build then succeeded. A successful retry is evidence of the race, not a fix.

The failure reproduced after repinning to `d2ff7887b06e3f763aa550e915a554881ce9700c` while other
consumers compiled against the same shared framework checkout. This time the generator explicitly
printed `shader bytes unchanged; refreshed dependency stamp` and `gen_gpu_shaders` completed before
Clang again reported the header missing from `gpu_vk.cpp`. An isolated-worktree consumer build had
completed successfully immediately beforehand. The cross-consumer difference is evidence that the
source-tree byproduct is being mutated across independent build graphs; the d2ff build was stopped
after the failure and is not final Toy provenance.

Framework `08ecc2605f7abdaa968d583efbe4215efb8f1d9d` moved the header into the consumer build tree and
therefore fixed the deletion race, but the first real nested consumer exposed a distinct integration
bug in that fix. Generation correctly wrote
`build/psxport_build/psxport_generated/gpu_vk_shaders.h`; both `gpu_vk.cpp` and
`rmlui_render_gpu.cpp` then failed to include `psxport_generated/gpu_vk_shaders.h` because psxport's
include path exposed `${CMAKE_BINARY_DIR}` (the game build root) rather than the nested framework
binary directory. A framework-root 81/81 gate did not cover this `add_subdirectory` topology. The
08ecc build was stopped and is not final Toy provenance; resolution requires a permanent nested
consumer gate as well as build-owned output.

## Ownership and next falsifier

The authoritative fix belongs in psxport's shader-generation dependency graph. Reproduce from a
clean consumer build while the ignored source-tree header is absent, including concurrent consumers
of the one shared framework checkout. The issue is resolved only when repeated clean parallel builds
cannot begin either `gpu_vk.cpp` or `rmlui_render_gpu.cpp` before the header exists. Do not add a Toy
Story build retry or checked-in generated header.

## Resolution

Pinned psxport `3418a79b624765614f3f198dc1e89632e1e650f0` gives each consumer build exclusive ownership of
`psxport_generated/gpu_vk_shaders.h`, exports the nested binary include directory, and permanently
tests the real `add_subdirectory` consumer topology. Its framework gate proves concurrent builds own
separate byte-identical headers, cleaning one preserves its peer, and the legacy shared-byproduct
fixture reproduces the opposite deletion answer. Toy Story then ran `cmake --build build --target
clean`, reconfigured from the clean shared framework with Clang, generated the header under its own
`build/psxport_build`, compiled both former failure sites, and linked both `toystory2_port` and the
boundary runner. The fix removes the shared owner rather than adding a retry.
