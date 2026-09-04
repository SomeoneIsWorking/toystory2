# Toy Story 2 title layer over psxport's native/dynarec runtime.

option(PSXPORT_BUILD_PORT "Build the Toy Story 2 native/dynarec product" ON)

include(${PSXPORT_DIR}/cmake/psxport.cmake)

set(TOYSTORY2_RUNTIME_SOURCES
  game/boot/guest_main_boot.cpp
  game/boot/native_sync_overrides.cpp
  game/core/game_config.cpp
  game/core/game_hooks.cpp
  game/core/guest_execution.cpp
  game/core/toystory2_context.cpp
  game/core/toystory2_runtime.cpp
  game/input/native_pad_owner.cpp
  game/loop/outer_loop.cpp
  game/loop/resident_frame.cpp
  game/loop/resident_preparation.cpp
  game/loop/toystory2_frame_driver.cpp
  game/render/guest_widescreen.cpp
  game/render/resident_camera_history.cpp
  game/render/resident_mesh_format.cpp
  game/render/resident_scene_history.cpp
)

function(toystory2_configure_target target)
  target_include_directories(${target} PRIVATE game game/core)
  target_link_libraries(${target} PRIVATE psxport)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endfunction()

if(PSXPORT_BUILD_PORT)
  add_executable(toystory2_port game/core/main.cpp ${TOYSTORY2_RUNTIME_SOURCES})
  toystory2_configure_target(toystory2_port)
  if(TARGET gen_gpu_shaders)
    add_dependencies(toystory2_port gen_gpu_shaders)
  endif()
  set_target_properties(toystory2_port PROPERTIES
    ENABLE_EXPORTS ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

if(NOT BUILD_TESTING)
  return()
endif()

add_executable(
  toystory2_projection_boundary
  tests/toystory2_projection_boundary.cpp
  game/core/game_config.cpp
  game/core/game_hooks.cpp
)
toystory2_configure_target(toystory2_projection_boundary)
target_include_directories(toystory2_projection_boundary PRIVATE ${PSXPORT_DIR}/tests)

add_executable(
  toystory2_cd_hle_boundary
  tests/toystory2_cd_hle_boundary.cpp
  game/core/game_config.cpp
  game/core/game_hooks.cpp
)
toystory2_configure_target(toystory2_cd_hle_boundary)
target_include_directories(toystory2_cd_hle_boundary PRIVATE ${PSXPORT_DIR}/tests)

add_executable(
  toystory2_frame_driver_boundary
  tests/toystory2_frame_driver_boundary.cpp
  ${TOYSTORY2_RUNTIME_SOURCES}
)
toystory2_configure_target(toystory2_frame_driver_boundary)
target_include_directories(toystory2_frame_driver_boundary PRIVATE ${PSXPORT_DIR}/tests)

foreach(target IN ITEMS
    toystory2_projection_boundary
    toystory2_cd_hle_boundary
    toystory2_frame_driver_boundary)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
endforeach()
