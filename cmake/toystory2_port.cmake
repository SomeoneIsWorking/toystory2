# cmake/toystory2_port.cmake — Toy Story 2 seam, generated substrate, port, and boundary gate.
#
# Three targets:
#
#   psxport            the framework static library. Always configured, so
#                      `cmake --build build --target psxport` works from a bare clone of this repo with
#                      nothing game-specific present. (psxport_smoke, the framework's agnosticism proof,
#                      CANNOT be built from a consumer tree — docs/issues/0001. Leave
#                      -DPSXPORT_BUILD_SMOKE at its OFF default.)
#   toystory2_seam     AN OBJECT LIBRARY over the game-owned runtime seam. It COMPILES but does not
#                      link, which proves ToyStory2Runtime and its bounded legacy facts still satisfy
#                      the framework without game-derived bytes. The registry's explicit
#                      no-substrate branch is what this target checks.
#   toystory2_port     the game binary. Configured ONLY when generated/rec_sources.cmake exists, i.e.
#                      once tools/recomp_substrate.py has emitted the identity-checked substrate. A
#                      fresh clone configures the seam first; run.sh provisions and emits before its
#                      configure, so the default route always builds the real product.

option(PSXPORT_BUILD_PORT "Build the Toy Story 2 native port binary (needs generated/)" ON)

# The framework static library. The root CMakeLists embeds psxport's normal root so its independent
# oracle tools are available; this include is deliberately idempotent.
include(${PSXPORT_DIR}/cmake/psxport.cmake)

# ---- the seam, compile-only ----------------------------------------------------------------------
set(SEAM_SRC
  game/core/game_config.cpp
  game/core/game_hooks.cpp
  game/core/main.cpp
  game/core/recomp_register.cpp
  game/core/toystory2_runtime.cpp
  game/sync/field_clock.cpp
)
add_library(toystory2_seam OBJECT ${SEAM_SRC})
set_target_properties(toystory2_seam PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(toystory2_seam PRIVATE game game/core)
# Links only for its INTERFACE include directories — an OBJECT library performs no link step, which is
# the whole point: no substrate is needed to check that the seam is well-formed.
target_link_libraries(toystory2_seam PRIVATE psxport)
target_compile_options(toystory2_seam PRIVATE -g)

if(NOT PSXPORT_BUILD_PORT)
  return()
endif()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/generated/rec_sources.cmake)
  message(STATUS
    "toystory2_port: NOT configured — generated/rec_sources.cmake is absent, i.e. the recompiled "
    "substrate is not provisioned in this checkout. Run `python3 tools/recomp_substrate.py --ensure` "
    "or `./run.sh`; the compile-only `toystory2_seam` remains available without game-derived bytes.")
  return()
endif()

# ---- the recompiled substrate --------------------------------------------------------------------
# emit.py writes the exact TU list to generated/rec_sources.cmake (GEN_REC_SRCS, basenames), so the set
# is deterministic — no globbing, which would wrongly pull unlinked stub TUs.
#
# -foptimize-sibling-calls IS REQUIRED, NOT an optimisation nicety: a guest TAIL JUMP is emitted as
# `dispatch(c,x); return;` in tail position and the guest uses such tail jumps for loops that iterate
# indefinitely. Without sibling-call optimisation each iteration becomes a real C call, the stack grows
# per loop, and the process SIGSEGVs.
include(${CMAKE_SOURCE_DIR}/generated/rec_sources.cmake)
list(TRANSFORM GEN_REC_SRCS PREPEND generated/)
set_source_files_properties(${GEN_REC_SRCS}
  PROPERTIES LANGUAGE CXX
  COMPILE_OPTIONS "-w;-O1;-foptimize-sibling-calls;-fno-strict-aliasing;-fwrapv")

add_library(toystory2_generated OBJECT ${GEN_REC_SRCS})
target_include_directories(toystory2_generated PRIVATE generated/)
target_link_libraries(toystory2_generated PRIVATE psxport)
set_target_properties(toystory2_generated PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(toystory2_port ${SEAM_SRC} $<TARGET_OBJECTS:toystory2_generated>)

# Select the real generated registry; the compile-only seam above keeps checking the loud
# no-substrate branch independently.
target_compile_definitions(toystory2_port PRIVATE TS2_HAVE_SUBSTRATE=1)

# The framework's SDL_GPU shader header is produced by a psxport custom target; gpu_vk.cpp (inside
# libpsxport) needs it present before this target's link ordering.
add_dependencies(toystory2_port gen_gpu_shaders)

set_target_properties(toystory2_port PROPERTIES
  CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
  ENABLE_EXPORTS ON                                    # -rdynamic: watchdog backtrace symbol names
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/scratch/bin)

# Only game/* include dirs here — the framework's (runtime, generated, vendored backends, SDL, freetype)
# are inherited PUBLICly from the psxport link below.
target_include_directories(toystory2_port PRIVATE game game/core generated/)

target_compile_options(toystory2_port PRIVATE -w -O2 -g
  ${SDL3_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})

target_link_libraries(toystory2_port PRIVATE psxport)

# Executes the real generated entry only until the instruction-derived first call. The Python gate
# compares all registers there against the independent Mednafen CPU oracle.
add_executable(
  toystory2_recomp_boundary
  tests/toystory2_recomp_boundary.cpp
  $<TARGET_OBJECTS:toystory2_generated>
)
target_include_directories(toystory2_recomp_boundary PRIVATE generated/)
target_compile_options(
  toystory2_recomp_boundary PRIVATE -O1 -foptimize-sibling-calls -fno-strict-aliasing -fwrapv
)
target_link_libraries(toystory2_recomp_boundary PRIVATE psxport)
# The boundary executable is only half of this gate. A clean documented build must also produce the
# independent oracle consumed by CTest; relying on a binary left by an older build made clean CTest
# refuse while incremental trees passed.
add_dependencies(toystory2_recomp_boundary oracle_trace)
set_target_properties(
  toystory2_recomp_boundary
  PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
             RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/scratch/bin
)

add_custom_target(
  toystory2_recomp_boundary_check
  COMMAND
    ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/compare_recomp_boundary.py
    --selftest --oracle $<TARGET_FILE:oracle_trace>
    --runner $<TARGET_FILE:toystory2_recomp_boundary>
  DEPENDS toystory2_recomp_boundary oracle_trace
  USES_TERMINAL
  VERBATIM
)
