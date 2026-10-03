// Hermetic boundary for the RESIDENT PRODUCERS: the native pad owner, the authored camera the 60 fps
// in-between pairs from, and the guest's own visibility and mesh submissions decoded from guest RAM.
// Each owner is exercised through its shipping interface. No guest execution, audio, GPU, disc, or
// window.

#include "fps60/camera_history.h"
#include "game.h"
#include "game_runtime.h"
#include "hw_bind.h"
#include "render/scene_history.h"
#include "runtime/toystory2_context.h"
#include "runtime/toystory2_runtime.h"
#include "testutil.h"

#include <memory>
#include <optional>

namespace {

static void test_native_pad_owner_publishes_and_decodes_digital_packet() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  game->pad.setButtons(0xBFFFu);
  ts2::context(core).pad.initialize(core);
  CHECK_EQ(core.mem_r16(0x800A109Cu), 1u);
  CHECK_EQ(core.mem_r8(0x800CF8A0u), 0u);
  CHECK_EQ(core.mem_r8(0x800CF8A1u), 0x41u);
  CHECK_EQ(core.mem_r16(0x800CF8A2u), 0xBFFFu);
  CHECK_EQ(core.mem_r8(0x800CF8C8u), 0xFFu);
  CHECK_EQ(ts2::context(core).pad.decode(core), 0x4000u);

  ts2::context(core).pad.shutdown(core);
  CHECK_EQ(core.mem_r16(0x800A109Cu), 0u);
  CHECK_EQ(ts2::context(core).pad.decode(core), 0u);
}

static void test_resident_camera_history_reads_authored_state_and_interpolates_wrap() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  core.mem_w32(0x800C1540u, static_cast<uint32_t>(-100));
  core.mem_w32(0x800C1544u, 200u);
  core.mem_w32(0x800C1548u, 300u);
  core.mem_w16(0x800C154Cu, 4090u);
  core.mem_w16(0x800C154Eu, 100u);
  core.mem_w16(0x800C1550u, 200u);

  ts2::ResidentCameraHistory &history = ts2::context(core).camera;
  history.capture(core);
  CHECK(history.ready());
  CHECK_EQ(history.previous().position[0], -100);
  CHECK_EQ(history.current().rotation[0], 4090u);

  ts2::ResidentCameraSample next = history.current();
  next.position[0] = 100;
  next.position[1] = 400;
  next.position[2] = 700;
  next.rotation[0] = 6;
  history.capture(next);

  const ts2::InterpolatedResidentCamera halfway = history.interpolate(0.5F);
  CHECK(halfway.position[0] == 0.0F);
  CHECK(halfway.position[1] == 300.0F);
  CHECK(halfway.position[2] == 500.0F);
  CHECK(halfway.rotation[0] == 4096.0F);
}

static void test_resident_scene_history_reads_exact_owner_and_mesh_arguments() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // The production observation snapshots the currently bound core's GTE controls before calling the
  // generated mesh submitter. Bind this fixture's Core before exercising that exact seam.
  gte_bind(&core);

  constexpr uint32_t visibilityList = 0x800B0000u;
  constexpr uint32_t visibility = 0x800B0100u;
  constexpr uint32_t object = 0x800B0200u;
  constexpr uint32_t instance = 0x800B0300u;
  constexpr uint32_t resource = 0x800B0400u;
  constexpr uint32_t mesh = 0x800B0500u;
  core.mem_w32(0x800A11E0u, 1u);
  core.mem_w32(visibilityList, visibility);
  core.mem_w8(visibility + 0x0Eu, 0x49u);
  core.mem_w8(visibility + 0x0Fu, 3u);
  core.mem_w32(visibility + 0x10u, object);
  core.mem_w32(object + 0x14u, instance);
  core.mem_w32(object + 0x1Cu, resource);
  core.mem_w32(instance + 0u, static_cast<uint32_t>(-100));
  core.mem_w32(instance + 4u, 200u);
  core.mem_w32(instance + 8u, 300u);
  core.mem_w8(instance + 0x18u, 0xA5u);
  core.mem_w8(instance + 0x19u, 0xE3u);
  core.mem_w32(instance + 0x1Cu, mesh);
  core.mem_w32(0x800A12F8u, 0x800B1000u);
  core.mem_w32(ts2::kResidentMaterialTableBase + ts2::kResidentInitialMaterialTableOffset, 0x01020304u);
  core.mem_w16(ts2::kResidentMaterialTableBase + ts2::kResidentInitialMaterialTableOffset + 8u, 0x0506u);
  core.mem_w32(ts2::kResidentMaterialTableBase + 0x120u, 0xAABBCCDDu);
  core.mem_w16(ts2::kResidentMaterialTableBase + 0x120u + 8u, 0xEEFFu);
  core.mem_w32(mesh, 2u);
  core.mem_w16(mesh + 4u, static_cast<uint16_t>(-1));
  core.mem_w16(mesh + 6u, 2u);
  core.mem_w16(mesh + 8u, 3u);
  core.mem_w16(mesh + 10u, 0x7C1Fu);
  constexpr uint32_t command = mesh + 4u + 2u * 8u;
  core.mem_w16(command, 0x1261u);
  core.mem_w16(command + 2u, 1u);
  core.mem_w32(command + 4u, 0x00010200u);
  core.mem_w32(command + 8u, 0x11223344u);
  core.mem_w32(command + 12u, 0x55667788u);
  constexpr uint32_t inheritedMaterialCommand = command + 16u;
  core.mem_w16(inheritedMaterialCommand, 0x8001u);
  core.mem_w16(inheritedMaterialCommand + 2u, 1u);
  core.mem_w32(inheritedMaterialCommand + 4u, 0x00010200u);
  core.mem_w32(inheritedMaterialCommand + 8u, 0x99AABBCCu);
  core.mem_w32(inheritedMaterialCommand + 12u, 0xDDEEFF00u);
  constexpr uint32_t terminalCommand = inheritedMaterialCommand + 16u;
  // A negative terminal avoids a material-state update. The preceding negative primitive must
  // inherit the preceding positive command's material state rather than reset it.
  core.mem_w16(terminalCommand, 0x8018u);
  core.mem_w16(terminalCommand + 2u, 0u);

  ts2::ResidentSceneHistory &history = ts2::context(core).scene;
  history.beginFrame();
  history.captureOwnerSubmission(core, visibilityList, 1u, 0x801BBFECu, 0u);
  history.captureMeshSubmission(core, mesh, 7u, 5u, 0x1F800384u);
  history.finishFrame();

  CHECK(history.ready());
  CHECK_EQ(history.current().batches().size(), 1u);
  CHECK_EQ(history.current().candidates().size(), 1u);
  CHECK_EQ(history.current().meshes().size(), 1u);
  const ts2::ResidentSceneCandidate &candidate = history.current().candidates()[0];
  CHECK_EQ(candidate.visibilityAddress, visibility);
  CHECK_EQ(candidate.objectAddress, object);
  CHECK_EQ(candidate.resourceAddress, resource);
  CHECK_EQ(candidate.instanceAddress, instance);
  CHECK_EQ(candidate.meshAddress, mesh);
  CHECK_EQ(candidate.position[0], -100);
  CHECK_EQ(candidate.position[1], 200);
  CHECK_EQ(candidate.position[2], 300);
  CHECK_EQ(candidate.type, 0x49u);
  CHECK_EQ(candidate.viewport, 3u);
  CHECK_EQ(candidate.material, 0xA5u);
  CHECK_EQ(candidate.scaleFlags, 0xE3u);
  CHECK_EQ(history.current().meshes()[0].meshAddress, mesh);
  const ts2::ResidentMeshSubmission &submission = history.current().meshes()[0];
  CHECK_EQ(submission.headerWord, 2);
  CHECK_EQ(submission.scale, 7u);
  CHECK_EQ(submission.materialDepthTableIndex, 5u);
  CHECK_EQ(submission.materialDepthTableAddress, 0x800B1014u);
  CHECK(submission.decoded);
  CHECK_EQ(submission.layout.vertexAddress, mesh + 4u);
  CHECK_EQ(submission.layout.vertexCount, 2u);
  CHECK_EQ(submission.layout.commandAddress, command);
  CHECK(!submission.layout.hasAuxiliaryVertexRecords);
  CHECK_EQ(submission.firstCommand.opcode, 1u);
  CHECK_EQ(submission.firstCommand.primitiveCount, 1);
  CHECK_EQ(submission.firstCommand.vertexCount, 3u);
  CHECK_EQ(submission.firstCommand.descriptorStride, 12u);
  CHECK_EQ(submission.firstCommand.materialTableOffset, 0x120u);
  CHECK_EQ(submission.firstCommand.blendVariant, 3u);
  CHECK_EQ(submission.firstPrimitive.vertexIndices[0], 0u);
  CHECK_EQ(submission.firstPrimitive.vertexIndices[1], 2u);
  CHECK_EQ(submission.firstPrimitive.vertexIndices[2], 1u);
  CHECK_EQ(submission.firstPrimitive.attributeWords[0], 0x11223344u);
  CHECK_EQ(submission.firstPrimitive.attributeWords[1], 0x55667788u);
  CHECK_EQ(submission.firstPrimitive.textureCoordinateCount, 3u);
  CHECK_EQ(submission.firstPrimitive.textureCoordinateWords[0], 0x1122u);
  CHECK_EQ(submission.firstPrimitive.textureCoordinateWords[1], 0x7788u);
  CHECK_EQ(submission.firstPrimitive.textureCoordinateWords[2], 0x5566u);
  CHECK_EQ(submission.commandSummary.commandCount, 3u);
  CHECK_EQ(submission.commandSummary.primitiveCount, 2u);
  CHECK_EQ(submission.commandSummary.primitiveOpcodeMask, 1u << 1u);
  CHECK_EQ(submission.commandSummary.materialTableSlotMask, 1u << 0x12u);
  CHECK_EQ(submission.commandSummary.blendVariantMask, 1u << 3u);
  CHECK_EQ(submission.commandSummary.terminalOpcode, 24u);
  CHECK_EQ(submission.materialCensus.commandCount, 3u);
  CHECK_EQ(submission.materialCensus.materialStateUpdateCount, 1u);
  CHECK_EQ(submission.materialCensus.descriptorCount, 2u);
  CHECK_EQ(submission.materialCensus.sampleCount, 2u);
  CHECK(!submission.materialCensus.descriptorSampleOverflow);
  CHECK(submission.materialCensus.complete);
  CHECK_EQ(submission.descriptorSamples[0].material.tableOffset, 0x120u);
  CHECK_EQ(submission.descriptorSamples[0].material.blendBits, 0x0060u);
  CHECK_EQ(submission.descriptorSamples[0].material.tableAddress, ts2::kResidentMaterialTableBase + 0x120u);
  CHECK_EQ(submission.descriptorSamples[0].material.tableWord0, 0xAABBCCDDu);
  CHECK_EQ(submission.descriptorSamples[0].material.tableWord8, 0xEEFFu);
  CHECK_EQ(submission.descriptorSamples[1].material.tableOffset, 0x120u);
  CHECK_EQ(submission.descriptorSamples[1].material.blendBits, 0x0060u);
  CHECK_EQ(submission.descriptorSamples[1].material.tableAddress, ts2::kResidentMaterialTableBase + 0x120u);
  CHECK_EQ(submission.descriptorSamples[1].material.tableWord0, 0xAABBCCDDu);
  CHECK_EQ(submission.descriptorSamples[1].material.tableWord8, 0xEEFFu);
  CHECK_EQ(history.previous().candidates()[0].meshAddress, mesh);

  const std::optional<ts2::ResidentMeshVertex> firstVertex = ts2::decodeResidentMeshVertex(core, submission.layout, 0u);
  CHECK(firstVertex.has_value());
  CHECK_EQ(firstVertex->x, -1);
  CHECK_EQ(firstVertex->y, 2);
  CHECK_EQ(firstVertex->z, 3);
  CHECK_EQ(firstVertex->color555, 0x7C1Fu);

  constexpr uint32_t compressedMesh = 0x800B0800u;
  core.mem_w32(compressedMesh, static_cast<uint32_t>(-2));
  constexpr uint32_t compressedCommand = compressedMesh + 8u + 2u * 12u;
  core.mem_w16(compressedCommand, 0x8018u);
  core.mem_w16(compressedCommand + 2u, 0u);
  const std::optional<ts2::ResidentMeshLayout> compressedLayout = ts2::decodeResidentMeshLayout(core, compressedMesh);
  CHECK(compressedLayout.has_value());
  CHECK_EQ(compressedLayout->vertexCount, 2u);
  CHECK(compressedLayout->hasAuxiliaryVertexRecords);
  CHECK_EQ(compressedLayout->commandAddress, compressedCommand);
  const std::optional<ts2::ResidentMeshCommand> terminal = ts2::decodeResidentMeshCommand(core, compressedCommand);
  CHECK(terminal.has_value());
  CHECK(terminal->terminal);
  CHECK_EQ(terminal->opcode, 24u);

  constexpr uint32_t lastResidentWord = 0x801FFFFCu;
  core.mem_w16(lastResidentWord, 0x001Fu);
  core.mem_w16(lastResidentWord + 2u, 0u);
  const std::optional<ts2::ResidentMeshCommand> finalWordTerminal =
      ts2::decodeResidentMeshCommand(core, lastResidentWord);
  CHECK(finalWordTerminal.has_value());
  CHECK(finalWordTerminal->terminal);
  CHECK_EQ(finalWordTerminal->opcode, 31u);

  // A stream that begins negative has no replacement state, so it must retain the submitter's
  // initial 0xE0 record. This is intentionally separate from the positive-then-negative stream.
  constexpr uint32_t initialStateMesh = 0x800B0C00u;
  core.mem_w32(initialStateMesh, 1u);
  constexpr uint32_t initialStateCommand = initialStateMesh + 4u + 8u;
  core.mem_w16(initialStateCommand, 0x8001u);
  core.mem_w16(initialStateCommand + 2u, 1u);
  core.mem_w32(initialStateCommand + 4u, 0x00000000u);
  core.mem_w32(initialStateCommand + 8u, 0x11111111u);
  core.mem_w32(initialStateCommand + 12u, 0x22222222u);
  core.mem_w16(initialStateCommand + 16u, 0x8018u);
  core.mem_w16(initialStateCommand + 18u, 0u);
  const std::optional<ts2::ResidentMeshLayout> initialStateLayout =
      ts2::decodeResidentMeshLayout(core, initialStateMesh);
  CHECK(initialStateLayout.has_value());
  std::array<ts2::ResidentMeshDescriptorSample, 1> initialStateSamples{};
  const ts2::ResidentMeshMaterialCensus initialStateCensus =
      ts2::censusResidentMeshMaterials(core, *initialStateLayout, initialStateSamples);
  CHECK(initialStateCensus.complete);
  CHECK_EQ(initialStateCensus.descriptorCount, 1u);
  CHECK_EQ(initialStateCensus.sampleCount, 1u);
  CHECK_EQ(initialStateSamples[0].material.tableOffset, ts2::kResidentInitialMaterialTableOffset);
  CHECK_EQ(initialStateSamples[0].material.blendBits, 0u);
  CHECK_EQ(initialStateSamples[0].material.tableAddress,
           ts2::kResidentMaterialTableBase + ts2::kResidentInitialMaterialTableOffset);
  CHECK_EQ(initialStateSamples[0].material.tableWord0, 0x01020304u);
  CHECK_EQ(initialStateSamples[0].material.tableWord8, 0x0506u);
}

} // namespace

int main() {
  RUN(native_pad_owner_publishes_and_decodes_digital_packet);
  RUN(resident_camera_history_reads_authored_state_and_interpolates_wrap);
  RUN(resident_scene_history_reads_exact_owner_and_mesh_arguments);
  return pt_summary();
}
