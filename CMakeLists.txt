# Toy Story 2 (SLUS-00893, USA) native port on the psxport framework.
cmake_minimum_required(VERSION 3.21)

# ccache, when available; must be set before project().
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  message(STATUS "ccache: ${CCACHE_PROGRAM}")
endif()

project(toystory2 CXX C)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

# PSXPORT_DIR is the framework source; default external/psxport, linked or cloned by
# tools/psxport_fetch.py. Override with -DPSXPORT_DIR=/path/to/psxport.
# Derive every framework path from it; never spell `external/psxport` at a call site.
set(PSXPORT_DIR "${CMAKE_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — "
                      "run `uv run --frozen python tools/psxport_fetch.py`, or point PSXPORT_DIR at a psxport checkout.")
endif()
message(STATUS "psxport framework: ${PSXPORT_DIR}")

# The framework's nested vendors are initialised by tools/psxport_fetch.py.
if(NOT EXISTS "${PSXPORT_DIR}/vendor/lucent/CMakeLists.txt")
  message(FATAL_ERROR "psxport's vendor/lucent is not checked out in the shared clone — run "
                      "`git -C ${PSXPORT_DIR} submodule update --init vendor/lucent` there once.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Embed psxport's root for its oracle/crt0 tools; framework tests stay off.
set(PSXPORT_BUILD_TESTS OFF CACHE BOOL "Register psxport's complete test suite" FORCE)
set(PSXPORT_BUILD_SMOKE OFF CACHE BOOL "Build the framework smoke" FORCE)
add_subdirectory(${PSXPORT_DIR} ${CMAKE_BINARY_DIR}/psxport_build)

include(cmake/toystory2_port.cmake)

if(BUILD_TESTING)
  # Gate: the C++ boundary tests, the --help contract and the C++ policy check.
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
