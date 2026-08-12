# Toy Story 2: Buzz Lightyear to the Rescue! (SLUS-00893, USA) — a PC-native port built on the psxport
# framework (external/psxport submodule).
#
# The framework (PSX-generic static-recomp substrate + runtime + HW backends + SBS harness + SDL_GPU
# renderer) is the reusable `psxport` library; this repo adds the game (game/**) and — once it exists —
# the recompiled SLUS_008.93 substrate (generated/**). NOTHING IS RE'd YET: see docs/re-frontier.md and
# cmake/toystory2_port.cmake for what does and does not build today.
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

# PSXPORT_DIR — WHERE THE FRAMEWORK SOURCE COMES FROM. Defaults to the pinned submodule, so a fresh
# clone of this repo alone builds standalone: that property is what "each game is its own project using
# psxport as the framework" means, and it must stay testable.
#
# Point it at the workspace's framework DEV CLONE to build against in-progress framework work WITHOUT
# editing the submodule (the submodule is a READ-ONLY pinned consumer; framework edits happen in one
# place only — external/psxport/docs/workspace/PROTOCOL.md):
#
#   cmake -S . -B build -DPSXPORT_DIR=/path/to/psxport
#
# It is a CACHE path, so it survives reconfigures. Everything else in this file and in cmake/ derives
# from it — never re-spell `external/psxport` at a call site, or the two halves of a build can come from
# different checkouts.
set(PSXPORT_DIR "${CMAKE_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: the external/psxport submodule)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — "
                      "run `git submodule update --init external/psxport`, or point PSXPORT_DIR at a psxport checkout.")
endif()
# The framework's OWN submodules, checked here rather than 200 lines into the framework. Initialising
# external/psxport does NOT populate them: `git submodule update --init --recursive` ABORTS on
# beetle-psx's url-less `deps/lightning/gnulib` (external/psxport/docs/workspace/KNOWN-DEFECT-sync-submodules.md),
# and scripts/sync-submodules.sh prints "all at recorded gitlinks" while vendor/lucent is still EMPTY.
# Without this guard the failure surfaces as a raw `add_subdirectory` / "does not contain a
# CMakeLists.txt" error from inside psxport, which tells a fresh-clone user nothing.
foreach(_psxport_vendor "vendor/lucent" "vendor/beetle-psx/deps/libchdr")
  if(NOT EXISTS "${PSXPORT_DIR}/${_psxport_vendor}/CMakeLists.txt")
    message(FATAL_ERROR
      "the psxport checkout at PSXPORT_DIR=${PSXPORT_DIR} is missing ${_psxport_vendor} — its own "
      "submodules are not populated, and this build needs them (lucent = the logger, libchdr = CHD "
      "access).\n"
      "  Fix it PER PATH, non-recursively (--recursive aborts on beetle-psx's url-less "
      "deps/lightning/gnulib, and sync-submodules.sh certifies pins it never checked):\n"
      "    git -C \"${PSXPORT_DIR}\" submodule update --init vendor/lucent vendor/beetle-psx")
  endif()
endforeach()
message(STATUS "psxport framework: ${PSXPORT_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ZLIB REQUIRED)
find_package(zstd CONFIG QUIET)
if(NOT TARGET zstd::libzstd_shared)
  find_library(ZSTD_LIBRARY zstd REQUIRED)
  add_library(zstd::libzstd_shared SHARED IMPORTED)
  set_target_properties(zstd::libzstd_shared PROPERTIES IMPORTED_LOCATION "${ZSTD_LIBRARY}")
endif()
find_package(Threads REQUIRED)

# libchdr (CHD access) comes from the Beetle PSX submodule NESTED under psxport (PSXPORT_DIR, above).
add_subdirectory(${PSXPORT_DIR}/vendor/beetle-psx/deps/libchdr
                 ${CMAKE_BINARY_DIR}/libchdr_build EXCLUDE_FROM_ALL)

include(cmake/toystory2_port.cmake)
