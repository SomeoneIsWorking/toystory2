# cmake/toystory2_port.cmake — what this repo builds, and what it deliberately refuses to build yet.
#
# Three targets:
#
#   psxport            the framework static library. Always configured, so
#                      `cmake --build build --target psxport` works from a bare clone of this repo with
#                      nothing game-specific present. (psxport_smoke, the framework's agnosticism proof,
#                      CANNOT be built from a consumer tree — docs/issues/0001. Leave
#                      -DPSXPORT_BUILD_SMOKE at its OFF default.)
#   toystory2_seam     AN OBJECT LIBRARY over the seam TUs (game_config / game_hooks / main). It COMPILES
#                      but does not link, which is exactly the check that is possible before a substrate
#                      exists: it proves this port's GameConfig/GameHooks still satisfy the pinned
#                      framework's seam — every designator binds, every hook signature matches. That is
#                      the gate for this repo today.
#   toystory2_port     the game binary. Configured ONLY when generated/rec_sources.cmake exists, i.e.
#                      once the recompiled substrate has been emitted. It has NOT been: emit.py needs
#                      this game's seeds (RE-02) and its OVERLAY LOAD BASES (RE-03 — this game has 21
#                      code overlays), and RE-01 has not produced a boot group. A loud STATUS message
#                      says so at configure time rather than a cryptic missing-file error, and rather
#                      than a stub binary that looks like a port.

option(PSXPORT_BUILD_PORT "Build the Toy Story 2 native port binary (needs generated/)" ON)

# The framework static library. Always included so `psxport` is buildable even when the game target is
# off. Never re-spell `external/psxport` here — ${PSXPORT_DIR} is the one spelling (CMakeLists.txt).
include(${PSXPORT_DIR}/cmake/psxport.cmake)

# ---- the seam, compile-only ----------------------------------------------------------------------
# recomp_register.cpp is EXCLUDED on purpose: it is the one TU that names generated/ symbols, so it
# cannot compile before the substrate exists (see that file's header).
set(SEAM_SRC
  game/core/game_config.cpp
  game/core/game_hooks.cpp
  game/core/main.cpp
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
    "substrate has never been emitted for this game. That is the honest state of this port, not a "
    "build problem: see docs/re-frontier.md (RE-01 crt0/GameConfig, RE-02 seeds, RE-03 the 22 code "
    "overlays' load bases — emit.py treats a missing overlay base as a hard error, deliberately). "
    "`--target toystory2_seam` is the gate that DOES run today.")
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
  COMPILE_OPTIONS "-O1;-foptimize-sibling-calls;-fno-strict-aliasing;-fwrapv")

add_executable(toystory2_port ${SEAM_SRC} game/core/recomp_register.cpp ${GEN_REC_SRCS})

# Tripwire, deliberately: recomp_register.cpp #errors under this define until someone writes the real
# RecompRegistry for the emitted substrate. The alternative — a registry written against guessed
# generated symbol names — is the kind of thing that reads as a framework bug months later.
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
target_include_directories(toystory2_port PRIVATE game game/core)

target_compile_options(toystory2_port PRIVATE -w -O2 -g
  ${SDL3_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})

target_link_libraries(toystory2_port PRIVATE psxport)
