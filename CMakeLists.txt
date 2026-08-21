# Toy Story 2: Buzz Lightyear to the Rescue! (SLUS-00893, USA) — a PC-native port built on the psxport
# framework (external/psxport resolves to the shared checkout or a private clone at psxport.pin).
#
# The framework (PSX-generic static-recomp substrate + runtime + HW backends + SBS harness + SDL_GPU
# renderer) is the reusable `psxport` library; this repo adds the game (game/**) and — once it exists —
# the recompiled SLUS_008.93 substrate (generated/**). Only the Ghidra supply (RE-00) is verified; see
# docs/re-frontier.md and cmake/toystory2_port.cmake for what does and does not build today.
#
# AND THERE IS NO DECOMP OF THIS GAME (docs/references.md), so there is no symbol map and no function
# boundaries to start from — every address this port will ever hold comes out of Ghidra on this
# executable. That does not change the build; it changes how long the ⬜ rows stay ⬜.
cmake_minimum_required(VERSION 3.21)

# ccache, when available. The recompiled substrate is hundreds of MB of generated C, so compilation is
# essentially the whole cost of an iteration. Must be set BEFORE project(), which is when CMake probes
# the compiler.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  message(STATUS "ccache: ${CCACHE_PROGRAM}")
endif()

project(toystory2 CXX C)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "Toy Story 2 C++ must use clang++; got ${CMAKE_CXX_COMPILER_ID} "
                      "(${CMAKE_CXX_COMPILER}). Reconfigure with CC=clang CXX=clang++.")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

# PSXPORT_DIR — WHERE THE FRAMEWORK SOURCE COMES FROM. Defaults to external/psxport, which is no
# longer a git submodule (2026-08-16): tools/psxport_sync.py --auto (called by run.sh) makes it a
# SYMLINK to the workspace's shared framework clone when one exists, or a private clone at this
# repo's psxport.pin on a fresh machine. So a fresh clone of this repo alone still builds standalone:
# that property is what "each game is its own project using psxport as the framework" means, and it
# must stay testable.
#
# Point it at a specific framework checkout to build against in-progress framework work:
#
#   cmake -S . -B build -DPSXPORT_DIR=/path/to/psxport
#
# It is a CACHE path, so it survives reconfigures. Everything else in this file and in cmake/ derives
# from it — never re-spell `external/psxport` at a call site, or the two halves of a build can come
# from different checkouts.
set(PSXPORT_DIR "${CMAKE_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — "
                      "run `python3 tools/psxport_sync.py --auto`, or point PSXPORT_DIR at a psxport checkout.")
endif()
message(STATUS "psxport framework: ${PSXPORT_DIR}")

# RECORD WHAT THIS CONFIGURE RESOLVED, so `tools/psxport_sync.py --check` can compare the framework
# you actually BUILT against with the one psxport.pin RECORDS. The framework is no longer a git
# submodule (external/psxport is a symlink to the workspace's shared clone, or a private clone at the
# pin — see tools/psxport_sync.py); the pin is provenance, and this file is what keeps it honest.
execute_process(COMMAND git -C "${PSXPORT_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE PSXPORT_RESOLVED_SHA OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET RESULT_VARIABLE _psxport_rev_rc)
if(NOT _psxport_rev_rc EQUAL 0)
  set(PSXPORT_RESOLVED_SHA "unknown")   # honest: never silently record a plausible-looking sha
endif()
file(WRITE "${CMAKE_BINARY_DIR}/psxport_resolved.txt"
     "dir = ${PSXPORT_DIR}\ncommit = ${PSXPORT_RESOLVED_SHA}\n")
message(STATUS "psxport commit: ${PSXPORT_RESOLVED_SHA}")

# The framework's OWN nested vendors (vendor/lucent, vendor/beetle-psx/deps/libchdr) are the SHARED
# clone's concern now: they were initialized once in the dev clone, and external/psxport is a symlink
# to it. A fresh machine's private clone at the pin gets them from tools/psxport_sync.py --clone (which
# inits them non-recursively). If they are somehow missing, the error comes from inside psxport with a
# clear "does not contain a CMakeLists.txt", and the fix is `git -C external/psxport submodule update
# --init <vendor>` in the dev clone.
if(NOT EXISTS "${PSXPORT_DIR}/vendor/lucent/CMakeLists.txt")
  message(FATAL_ERROR "psxport's vendor/lucent is not checked out in the shared clone — run "
                      "`git -C ${PSXPORT_DIR} submodule update --init vendor/lucent` there once.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Embed psxport's normal root so this consumer gets the shipping oracle/crt0 tools as well as the
# library. Framework tests stay off; this repo owns its own CTest surface.
set(PSXPORT_BUILD_TESTS OFF CACHE BOOL "Register psxport's complete test suite" FORCE)
set(PSXPORT_BUILD_SMOKE OFF CACHE BOOL "Build the framework smoke" FORCE)
add_subdirectory(${PSXPORT_DIR} ${CMAKE_BINARY_DIR}/psxport_build)

include(cmake/toystory2_port.cmake)

if(BUILD_TESTING)
  add_test(NAME run_py_selftest COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/run.py --selftest)
  add_test(
    NAME verify_crt0_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/verify_crt0.py --selftest
  )
  add_test(
    NAME overlay_map_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/overlay_map.py --selftest
  )
  if(TARGET toystory2_recomp_boundary)
    add_test(
      NAME recomp_substrate_selftest
      COMMAND
        ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/recomp_substrate.py --selftest
    )
    add_test(
      NAME recomp_boundary_selftest
      COMMAND
        ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/compare_recomp_boundary.py
        --selftest --oracle $<TARGET_FILE:oracle_trace>
        --runner $<TARGET_FILE:toystory2_recomp_boundary>
    )
  endif()
  add_test(
    NAME cpp_policy
    COMMAND
      ${Python3_EXECUTABLE} ${PSXPORT_DIR}/tools/check_cpp_style.py
      --root ${CMAKE_SOURCE_DIR}
      --compile-commands ${CMAKE_BINARY_DIR}/compile_commands.json
  )
endif()
