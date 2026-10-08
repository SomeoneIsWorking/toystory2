// Resident producers boundary: the native pad owner, the slot-mesh and actor producers' keys and the frame cut,
// each through the shipping override with a stub guest body.

#include "frame/frame_cut.h"
#include "game.h"
#include "game_runtime.h"
#include "hw_bind.h"
#include "native_dispatch.h"
#include "render/actor_incarnation.h"
#include "render/actor_producers.h"
#include "render/part_face_drawers.h"
#include "render/slot_mesh_producers.h"
#include "runtime/toystory2_context.h"
#include "runtime/toystory2_runtime.h"
#include "testutil.h"

#include <memory>
#include <optional>

namespace {

using psx::present::RecordKey;

constexpr uint32_t kReturn = 0x80010100u;
constexpr uint32_t kJrRa = 0x03E00008u;
constexpr uint32_t kMesh = 0x800B0500u;
constexpr uint32_t kTable = 0x800B1000u;
constexpr uint32_t kOtherTable = 0x800B1100u;
constexpr uint32_t kPackets = 0x801E0000u;
constexpr uint32_t kPoolPacket = 0x801E8000u;
constexpr uint32_t kSwZeroA1 = 0xACA00000u;

std::unique_ptr<Game> residentGame() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  game->core.imageCatalog().activate("resident-test", {0x10000u, 0xA1800u}, 0x5453325245534944ull);
  return game;
}

void returnStub(Core &core, uint32_t entry) {
  core.mem_w32(entry, kJrRa);
  core.mem_w32(entry + 4u, 0u);
}

bool call(Core &core, uint32_t entry) {
  core.r[29] = 0x801FFF00u;
  core.r[31] = kReturn;
  return psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(1000000u)).returned();
}

// A body that writes one word at a1, as the submitter writes packets outside the slot array.
void storingStub(Core &core, uint32_t entry) {
  core.mem_w32(entry, kSwZeroA1);
  returnStub(core, entry + 4u);
}

void writeCommand(Core &core, uint32_t address, uint16_t opcode, uint16_t count) {
  core.mem_w16(address, opcode);
  core.mem_w16(address + 2u, count);
}

// Two vertices, then a triangle command of two primitives, a two-packet command of one, and the terminal.
void writeMesh(Core &core) {
  core.mem_w32(kMesh, 2u);
  const uint32_t triangles = kMesh + 4u + 2u * 8u;
  writeCommand(core, triangles, 0x0001u, 2u);
  const uint32_t doublePacket = triangles + 4u + 2u * 12u;
  writeCommand(core, doublePacket, 0x0008u, 1u);
  writeCommand(core, doublePacket + 4u + 12u, 0x8018u, 0u);
}

void writeSlots(Core &core, uint32_t table, std::initializer_list<uint16_t> slots) {
  uint32_t entry = 0;
  for (const uint16_t slot : slots) {
    core.mem_w16(table + entry * 2u, slot);
    ++entry;
  }
}

bool submit(Core &core, uint32_t submitter, uint32_t table) {
  core.mem_w32(ts2::SlotMeshProducers::kInstanceSlotTable, table);
  core.mem_w32(ts2::SlotMeshProducers::kSlotPacketBase, kPackets);
  core.r[4] = kMesh;
  return call(core, submitter);
}

std::optional<RecordKey> keyOfSlot(Core &core, uint32_t slot) {
  return core.emission.identityFor(kPackets + slot * 4u);
}

bool keyedAs(Core &core, uint32_t slot, uint32_t producer, uint32_t table, uint32_t entry) {
  return keyOfSlot(core, slot) == RecordKey{producer, table, entry, 0u};
}

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

// Opcodes 8..15 of the static submitter draw two packets per primitive; the rigid drawer draws one.
static void test_slot_entries_follow_each_submitters_command_stream() {
  auto game = residentGame();
  Core &core = game->core;
  writeMesh(core);
  CHECK_EQ(ts2::SlotMeshProducers::slotEntries(core, ts2::SlotMeshProducers::kStaticMeshSubmitter, kMesh), 4u);
  CHECK_EQ(ts2::SlotMeshProducers::slotEntries(core, ts2::SlotMeshProducers::kRigidMeshDrawer, kMesh), 3u);
  core.mem_w16(kMesh + 4u + 2u * 8u + 2u, 0u);
  CHECK_EQ(ts2::SlotMeshProducers::slotEntries(core, ts2::SlotMeshProducers::kStaticMeshSubmitter, kMesh), 0u);
}

// A face's packet is keyed by its instance's slot table and its entry index, whichever slot holds it.
static void test_static_mesh_faces_are_keyed_by_instance_and_entry() {
  auto game = residentGame();
  Core &core = game->core;
  returnStub(core, ts2::SlotMeshProducers::kStaticMeshSubmitter);
  returnStub(core, ts2::SlotMeshProducers::kRigidMeshDrawer);
  ts2::SlotMeshProducers::install(core);
  writeMesh(core);
  constexpr uint32_t kProducer = ts2::SlotMeshProducers::kStaticMeshSubmitter;

  writeSlots(core, kTable, {3u, 0u, 7u, 9u});
  CHECK(submit(core, kProducer, kTable));
  CHECK(keyedAs(core, 3u, kProducer, kTable, 0u));
  CHECK(keyedAs(core, 7u, kProducer, kTable, 2u));
  CHECK(keyedAs(core, 9u, kProducer, kTable, 3u));
  CHECK(!keyOfSlot(core, 5u).has_value());

  // Next frame: face 0 now in slot 5, face 2 culled and unwalked; another instance of the same mesh is another object.
  writeSlots(core, kTable, {5u, 0u, 0u, 9u});
  CHECK(submit(core, kProducer, kTable));
  writeSlots(core, kOtherTable, {11u, 12u, 0u, 0u});
  CHECK(submit(core, kProducer, kOtherTable));
  CHECK(keyedAs(core, 5u, kProducer, kTable, 0u));
  CHECK(keyedAs(core, 9u, kProducer, kTable, 3u));
  CHECK(keyedAs(core, 11u, kProducer, kOtherTable, 0u));
  CHECK(keyedAs(core, 12u, kProducer, kOtherTable, 1u));
}

// Only the faces the slot table names are keyed; any other packet the body writes has no identity.
static void test_packets_outside_the_slot_array_stay_unkeyed() {
  auto game = residentGame();
  Core &core = game->core;
  storingStub(core, ts2::SlotMeshProducers::kStaticMeshSubmitter);
  returnStub(core, ts2::SlotMeshProducers::kRigidMeshDrawer);
  ts2::SlotMeshProducers::install(core);
  writeMesh(core);

  writeSlots(core, kTable, {3u, 0u, 0u, 0u});
  core.r[5] = kPoolPacket;
  CHECK(submit(core, ts2::SlotMeshProducers::kStaticMeshSubmitter, kTable));
  CHECK(keyedAs(core, 3u, ts2::SlotMeshProducers::kStaticMeshSubmitter, kTable, 0u));
  CHECK(!core.emission.identityFor(kPoolPacket).has_value());
}

static void test_rigid_mesh_faces_are_keyed_by_the_rigid_drawer() {
  auto game = residentGame();
  Core &core = game->core;
  returnStub(core, ts2::SlotMeshProducers::kStaticMeshSubmitter);
  returnStub(core, ts2::SlotMeshProducers::kRigidMeshDrawer);
  ts2::SlotMeshProducers::install(core);
  writeMesh(core);
  constexpr uint32_t kProducer = ts2::SlotMeshProducers::kRigidMeshDrawer;

  writeSlots(core, kTable, {4u, 6u, 8u, 10u});
  CHECK(submit(core, kProducer, kTable));
  CHECK(keyedAs(core, 4u, kProducer, kTable, 0u));
  CHECK(keyedAs(core, 8u, kProducer, kTable, 2u));
  // Three entries: the fourth word is not this mesh's.
  CHECK(!keyOfSlot(core, 10u).has_value());
}

constexpr uint32_t kActor = 0x800C2F38u;
constexpr uint32_t kOtherActor = 0x800C2FD0u;
constexpr uint32_t kPartIndex = 3u;
constexpr uint32_t kPart = ts2::ActorProducers::kPartTable + kPartIndex * ts2::ActorProducers::kPartBytes;
constexpr uint32_t kFaces = 0x800B4000u;
constexpr uint32_t kOt = 0x800B6000u;
constexpr uint32_t kBuckets = 0x800B6100u;
constexpr uint32_t kPool = 0x801E0000u;
constexpr uint32_t kCursor = 0x800A1608u;
constexpr uint32_t kNearZ = 0u;
constexpr uint32_t kFarZ = 6000u; // SZ 7000: past the depth limit, so the face is culled

// The renderer's body for the test: hand the test part to `drawer`, as 0x8002518C does per part.
void rendererStub(Core &core, uint32_t drawer) {
  const uint32_t words[] = {0x27BDFFF8u,
                            0xAFBF0000u,
                            0x3C040000u | (kPart >> 16),
                            0x0C000000u | ((drawer >> 2) & 0x03FFFFFFu),
                            0x34840000u | (kPart & 0xFFFFu),
                            0x8FBF0000u,
                            kJrRa,
                            0x27BD0008u};
  uint32_t at = ts2::ActorProducers::kActorRenderer;
  for (const uint32_t word : words) {
    core.mem_w32(at, word);
    at += 4u;
  }
}

uint32_t screenXy(uint32_t x, uint32_t y) {
  return (y << 16) | x;
}

uint32_t writeTriangle(Core &core, uint32_t at, uint32_t z) {
  const uint32_t words[] = {
      0x34808080u, screenXy(0, 0), z, screenXy(10, 0), z, screenXy(0, 10), z, 0x00808080u, 0x80808080u};
  for (const uint32_t word : words) {
    core.mem_w32(at, word);
    at += 4u;
  }
  return at;
}

uint32_t writeQuad(Core &core, uint32_t at, uint32_t z) {
  const uint32_t words[] = {0x3C808080u,
                            screenXy(0, 0),
                            z,
                            screenXy(10, 0),
                            z,
                            screenXy(10, 10),
                            z,
                            screenXy(0, 10),
                            z,
                            0x00808080u,
                            0x00808080u,
                            0x80808080u};
  for (const uint32_t word : words) {
    core.mem_w32(at, word);
    at += 4u;
  }
  return at;
}

// Identity rotation, the part 1000 ahead, every row visible, buckets [0x14, 0x200) all at kOt.
void prepareActorScene(Core &core, uint32_t drawer) {
  rendererStub(core, drawer);
  returnStub(core, ts2::ActorIncarnations::kActorReset);
  ts2::ActorIncarnations::install(core);
  ts2::ActorProducers::install(core);
  core.mem_w32(kPart + 0x20u, kFaces);
  core.mem_w32(0x800A1100u, 240u);
  core.mem_w32(0x800A135Cu, 0x200u);
  core.mem_w32(0x800A12F8u, kBuckets);
  core.mem_w32(0x800A10BCu, kOt);
  core.mem_w32(0x800A13B4u, 0u);
  gte_bind(&core);
  const uint32_t control[][2] = {{0u, 0x1000u},
                                 {1u, 0u},
                                 {2u, 0x1000u},
                                 {3u, 0u},
                                 {4u, 0x1000u},
                                 {5u, 0u},
                                 {6u, 0u},
                                 {7u, 1000u},
                                 {24u, 0u},
                                 {25u, 16u << 16},
                                 {26u, 256u},
                                 {29u, 0x100u},
                                 {30u, 0x100u}};
  for (const auto &[reg, value] : control) {
    gte_write_ctrl(reg, value);
  }
}

bool renderActor(Core &core, uint32_t actor) {
  core.mem_w32(kCursor, kPool);
  core.mem_w32(kOt, 0x00FFFFFFu);
  core.r[4] = actor;
  return call(core, ts2::ActorProducers::kActorRenderer);
}

RecordKey faceKey(uint32_t actor, uint32_t generation, uint32_t face) {
  return RecordKey{
      ts2::ActorProducers::kActorRenderer, ts2::incarnationObject(actor, generation), (kPartIndex << 16) | face, 0u};
}

// A face culled before it reaches the pool consumes no packet, yet the next face keeps its own index.
static void test_actor_faces_are_keyed_by_model_face_not_packet_order() {
  auto game = residentGame();
  Core &core = game->core;
  prepareActorScene(core, ts2::PartFaceDrawers::kPrelit);
  uint32_t at = writeTriangle(core, kFaces, kFarZ);
  at = writeTriangle(core, at, kNearZ);
  core.mem_w32(at, 0u);

  CHECK(renderActor(core, kActor));
  CHECK_EQ(core.mem_r32(kCursor), kPool + 0x28u);
  CHECK_EQ(core.mem_r32(kOt), kPool & 0x00FFFFFFu);
  CHECK(core.emission.identityFor(kPool) == faceKey(kActor, 0u, 1u));
}

static void test_lit_quads_and_triangles_keep_their_model_indices() {
  auto game = residentGame();
  Core &core = game->core;
  prepareActorScene(core, ts2::PartFaceDrawers::kLit);
  uint32_t at = writeQuad(core, kFaces, kNearZ);
  at = writeTriangle(core, at, kFarZ);
  at = writeQuad(core, at, kNearZ);
  at = writeTriangle(core, at, kNearZ);
  core.mem_w32(at, 0u);

  CHECK(renderActor(core, kActor));
  CHECK_EQ(core.mem_r32(kCursor), kPool + 0x34u + 0x34u + 0x28u);
  CHECK(core.emission.identityFor(kPool) == faceKey(kActor, 0u, 0u));
  CHECK(core.emission.identityFor(kPool + 0x34u) == faceKey(kActor, 0u, 2u));
  CHECK(core.emission.identityFor(kPool + 0x68u) == faceKey(kActor, 0u, 3u));
}

// The guest's reset of a pool record starts a new life: its faces stop pairing with the old life's.
static void test_a_reset_actor_gets_a_new_key() {
  auto game = residentGame();
  Core &core = game->core;
  prepareActorScene(core, ts2::PartFaceDrawers::kPrelit);
  core.mem_w32(writeTriangle(core, kFaces, kNearZ), 0u);

  CHECK(renderActor(core, kActor));
  CHECK(core.emission.identityFor(kPool) == faceKey(kActor, 0u, 0u));

  core.r[4] = kOtherActor;
  CHECK(call(core, ts2::ActorIncarnations::kActorReset));
  CHECK(renderActor(core, kActor));
  CHECK(core.emission.identityFor(kPool) == faceKey(kActor, 0u, 0u));

  core.r[4] = kActor;
  CHECK(call(core, ts2::ActorIncarnations::kActorReset));
  CHECK(renderActor(core, kActor));
  CHECK(core.emission.identityFor(kPool) == faceKey(kActor, 1u, 0u));
  CHECK(!(faceKey(kActor, 1u, 0u) == faceKey(kActor, 0u, 0u)));
}

constexpr uint32_t kUnportedDrawer = 0x80031C3Cu;

// An unported drawer's packet store reaching mem_w (a Lightrec slow-path store) while an actor is drawn.
void unportedDrawer(Core *core) {
  core->mem_w32(kPool, 0x0C000000u);
  core->r[2] = 0u;
}

// Only ported drawers key packets: a store from any other drawer under the renderer binds nothing.
static void test_unported_drawer_packets_stay_unkeyed() {
  auto game = residentGame();
  Core &core = game->core;
  prepareActorScene(core, kUnportedDrawer);
  returnStub(core, kUnportedDrawer);
  psx::cpu::installNativeOverride(core, kUnportedDrawer, "unported-drawer", &unportedDrawer);

  CHECK(renderActor(core, kActor));
  CHECK(!core.emission.identityFor(kPool).has_value());
}

bool passIsCut(Core &core) {
  ts2::context(core).frameCut.notePassEnded(core);
  return core.game->runtime->sealedFrameIsCut(core);
}

bool cameraUpdate(Core &core, bool lookCamera, int32_t blendFields) {
  core.mem_w16(ts2::FrameCut::kCameraSource, lookCamera ? 1u : 0u);
  core.mem_w32(ts2::FrameCut::kCameraBlendFields, static_cast<uint32_t>(blendFields));
  return call(core, ts2::FrameCut::kCameraUpdate);
}

static void test_frame_cut_follows_the_guests_scene_and_camera_state() {
  auto game = residentGame();
  Core &core = game->core;
  returnStub(core, ts2::FrameCut::kCameraPlacement);
  returnStub(core, ts2::FrameCut::kCameraUpdate);
  ts2::FrameCut::install(core);
  core.mem_w32(ts2::FrameCut::kLevelId, 1u);
  core.mem_w32(ts2::FrameCut::kAreaObjects, 0x80149F94u);

  CHECK(passIsCut(core)); // nothing before the first pass
  CHECK(cameraUpdate(core, false, 0));
  CHECK(!passIsCut(core));
  CHECK(!passIsCut(core));

  // The camera placed afresh (level start, respawn) is a cut for one pass.
  CHECK(call(core, ts2::FrameCut::kCameraPlacement));
  CHECK(passIsCut(core));
  CHECK(!passIsCut(core));

  // The look camera taken with no blend pending is a cut; with a blend pending it is not.
  CHECK(cameraUpdate(core, true, 0));
  CHECK(passIsCut(core));
  CHECK(cameraUpdate(core, true, 0));
  CHECK(!passIsCut(core));
  CHECK(cameraUpdate(core, false, 0x40));
  CHECK(!passIsCut(core));

  // Another area or level is another scene.
  core.mem_w32(ts2::FrameCut::kAreaObjects, 0x8014A000u);
  CHECK(passIsCut(core));
  CHECK(!passIsCut(core));
  core.mem_w32(ts2::FrameCut::kLevelId, 2u);
  CHECK(passIsCut(core));
}

} // namespace

int main() {
  RUN(native_pad_owner_publishes_and_decodes_digital_packet);
  RUN(slot_entries_follow_each_submitters_command_stream);
  RUN(static_mesh_faces_are_keyed_by_instance_and_entry);
  RUN(packets_outside_the_slot_array_stay_unkeyed);
  RUN(rigid_mesh_faces_are_keyed_by_the_rigid_drawer);
  RUN(actor_faces_are_keyed_by_model_face_not_packet_order);
  RUN(lit_quads_and_triangles_keep_their_model_indices);
  RUN(a_reset_actor_gets_a_new_key);
  RUN(unported_drawer_packets_stay_unkeyed);
  RUN(frame_cut_follows_the_guests_scene_and_camera_state);
  return pt_summary();
}
