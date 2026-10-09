# Toy Story 2 title layer over psxport's native/dynarec runtime.

option(PSXPORT_BUILD_PORT "Build the Toy Story 2 native/dynarec product" ON)

include(${PSXPORT_DIR}/cmake/psxport.cmake)

set(TOYSTORY2_RUNTIME_SOURCES
  game/audio/sound_bank.cpp
  game/boot/guest_main_boot.cpp
  game/boot/graphics_sync.cpp
  game/boot/level_start_presentation.cpp
  game/boot/title_session.cpp
  game/cd/file_transfer.cpp
  game/execution/guest_execution.cpp
  game/frame/field_call.cpp
  game/frame/frame_cut.cpp
  game/frame/frame_boundary.cpp
  game/frame/frame_driver.cpp
  game/frame/outer_loop.cpp
  game/frame/resident_frame.cpp
  game/frame/resident_preparation.cpp
  game/fmv/movie_player.cpp
  game/input/pad_owner.cpp
  game/input/recording_phase.cpp
  game/overlay/overlay_images.cpp
  game/overlay/overlay_slot.cpp
  game/render/actor_incarnation.cpp
  game/render/actor_producers.cpp
  game/render/mesh_format.cpp
  game/render/ordering_tables.cpp
  game/render/part_draw_state.cpp
  game/render/part_face_drawers.cpp
  game/render/slot_mesh_producers.cpp
  game/runtime/toystory2_context.cpp
  game/runtime/toystory2_runtime.cpp
  game/widescreen/guest_widescreen.cpp
  game/widescreen/resident_widescreen.cpp
)

function(toystory2_configure_target target)
  target_include_directories(${target} PRIVATE game)
  target_link_libraries(${target} PRIVATE psxport)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endfunction()

if(PSXPORT_BUILD_PORT)
  add_executable(toystory2_port game/main.cpp ${TOYSTORY2_RUNTIME_SOURCES})
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
  ${TOYSTORY2_RUNTIME_SOURCES}
)
toystory2_configure_target(toystory2_projection_boundary)
target_include_directories(toystory2_projection_boundary PRIVATE ${PSXPORT_DIR}/tests)

add_executable(
  toystory2_cd_hle_boundary
  tests/toystory2_cd_hle_boundary.cpp
  ${TOYSTORY2_RUNTIME_SOURCES}
)
toystory2_configure_target(toystory2_cd_hle_boundary)
target_include_directories(toystory2_cd_hle_boundary PRIVATE ${PSXPORT_DIR}/tests)

add_executable(
  toystory2_level_start_card_boundary
  tests/toystory2_level_start_card_boundary.cpp
  ${TOYSTORY2_RUNTIME_SOURCES}
)
toystory2_configure_target(toystory2_level_start_card_boundary)
target_include_directories(toystory2_level_start_card_boundary PRIVATE ${PSXPORT_INCLUDE_DIRS} ${PSXPORT_DIR}/tests)

add_executable(
  toystory2_frame_turn_boundary
  tests/frame_turn_boundary.cpp
  ${TOYSTORY2_RUNTIME_SOURCES}
)
toystory2_configure_target(toystory2_frame_turn_boundary)
target_include_directories(toystory2_frame_turn_boundary PRIVATE ${PSXPORT_DIR}/tests)

add_executable(
  toystory2_resident_producers_boundary
  tests/resident_producers_boundary.cpp
  ${TOYSTORY2_RUNTIME_SOURCES}
)
toystory2_configure_target(toystory2_resident_producers_boundary)
target_include_directories(toystory2_resident_producers_boundary PRIVATE ${PSXPORT_DIR}/tests)

add_executable(
  toystory2_execution_boundary
  tests/toystory2_execution_boundary.cpp
  ${TOYSTORY2_RUNTIME_SOURCES}
)
toystory2_configure_target(toystory2_execution_boundary)
target_include_directories(toystory2_execution_boundary PRIVATE ${PSXPORT_DIR}/tests)

foreach(target IN ITEMS
    toystory2_projection_boundary
    toystory2_cd_hle_boundary
    toystory2_frame_turn_boundary
    toystory2_resident_producers_boundary
    toystory2_execution_boundary)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
endforeach()
