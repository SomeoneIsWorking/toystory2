#include "render/actor_producers.h"

#include "core.h"
#include "execution_exit.h"
#include "native_dispatch.h"
#include "render/part_face_drawers.h"
#include "runtime/toystory2_context.h"

#include <cstdlib>
#include <lucent/log.h>

namespace ts2 {
namespace {

template <std::uint32_t Renderer> void renderActor(Core *core) {
  ToyStory2Context &game = context(*core);
  game.actorProducers.setDrawing(game.actorIncarnations.object(core->r[4]));
  psx::cpu::callOriginalToReturn(*core, Renderer, psx::cpu::ExecutionBudget::currentTurn(*core), "actor producer");
  game.actorProducers.setDrawing(std::nullopt);
}

} // namespace

void ActorProducers::install(Core &core) {
  psx::cpu::installNativeOverride(core, kActorRenderer, "actor-renderer", &renderActor<kActorRenderer>);
  psx::cpu::installNativeOverride(
      core, kScaledActorRenderer, "scaled-actor-renderer", &renderActor<kScaledActorRenderer>);
  PartFaceDrawers::install(core);
}

std::uint32_t ActorProducers::faceElement(std::uint32_t part, std::uint32_t face) {
  if (part < kPartTable || (part - kPartTable) % kPartBytes != 0u || face >= kFaceIndexLimit) {
    lucent::error("ts2-actors", "part 0x{:08X} face {} is outside the part table's element space", part, face);
    std::abort();
  }
  return (((part - kPartTable) / kPartBytes) << 16) | face;
}

} // namespace ts2
