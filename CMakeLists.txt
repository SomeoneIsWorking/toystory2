# Toy Story 2: Buzz Lightyear to the Rescue! (SLUS-00893, USA) — a PC-native port built on the psxport
# framework (external/psxport resolves to the workspace's live checkout, or a clone of its main).
#
# The framework owns PSX hardware and runtime translation; this repo owns the authenticated title
# facts and native subsystems. The user's executable remains runtime data.
#
# AND THERE IS NO DECOMP OF THIS GAME (CLAUDE.md), so there is no symbol map and no function
# boundaries to start from — every address this port will ever hold comes out of Ghidra on this
# executable. That does not change the build; it changes how long the ⬜ rows stay ⬜.
cmake_minimum_required(VERSION 3.21)

# ccache, when available. This must be set before project(), when CMake probes the compiler.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  message(STATUS "ccache: ${CCACHE_PROGRAM}")
endif()

project(toystory2 CXX C)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

# PSXPORT_DIR — WHERE THE FRAMEWORK SOURCE COMES FROM. Defaults to external/psxport, which is no
# longer a git submodule (2026-08-16): tools/psxport_fetch.py (called by run.sh) makes it a link to
# the workspace's live framework checkout, or clones psxport main on a machine that has none. So a
# fresh clone of this repo alone still builds standalone: that property is what "each game is its own
# project using psxport as the framework" means, and it must stay testable.
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
                      "run `uv run --frozen python tools/psxport_fetch.py`, or point PSXPORT_DIR at a psxport checkout.")
endif()
message(STATUS "psxport framework: ${PSXPORT_DIR}")

# The framework's OWN nested vendors (vendor/lucent, vendor/beetle-psx/deps/libchdr) are the checkout's
# concern: tools/psxport_fetch.py initialises them non-recursively wherever it resolved the framework
# from. If they are somehow missing, the error comes from inside psxport with a clear "does not contain a
# CMakeLists.txt", and the fix is `git -C external/psxport submodule update --init <vendor>` in that
# checkout.
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
  # The gate is the product: the four C++ hermetic boundary tests, the executable's own --help
  # contract, and the C++ policy check (clang-format, clang-tidy, cpp_policy). Nothing here tests a tool.
  if(TARGET toystory2_projection_boundary)
    add_test(NAME projection_publication_boundary COMMAND toystory2_projection_boundary)
  endif()
  if(TARGET toystory2_cd_hle_boundary)
    add_test(NAME stock_libcd_native_boundary COMMAND toystory2_cd_hle_boundary)
  endif()
  if(TARGET toystory2_frame_turn_boundary)
    add_test(NAME frame_turn_boundary COMMAND toystory2_frame_turn_boundary)
  endif()
  if(TARGET toystory2_resident_producers_boundary)
    add_test(NAME resident_producers_boundary COMMAND toystory2_resident_producers_boundary)
  endif()
  if(TARGET toystory2_execution_boundary)
    add_test(NAME title_execution_boundary COMMAND toystory2_execution_boundary)
  endif()
  if(TARGET toystory2_port)
    foreach(_help_arg IN ITEMS -h --help)
      string(REPLACE "-" "" _help_name "${_help_arg}")
      add_test(NAME "product_help_${_help_name}" COMMAND toystory2_port "${_help_arg}")
      set_tests_properties(
        "product_help_${_help_name}"
        PROPERTIES
          ENVIRONMENT "PSXPORT_TS2_DISC=/disc-discovery-must-not-run.chd"
          PASS_REGULAR_EXPRESSION "usage:.*\\[PS-X EXE\\]"
      )
    endforeach()
  endif()
  add_test(
    NAME cpp_policy
    COMMAND
      ${Python3_EXECUTABLE} ${PSXPORT_DIR}/tools/check_cpp_style.py
      --root ${CMAKE_SOURCE_DIR}
      --compile-commands ${CMAKE_BINARY_DIR}/compile_commands.json
  )
endif()
