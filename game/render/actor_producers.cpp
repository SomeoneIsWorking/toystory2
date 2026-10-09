#include "render/actor_producers.h"

#include "core.h"
#include "execution_exit.h"
#include "native_dispatch.h"
#include "render/actor_incarnation.h"
#include "render/part_draw_state.h"
#include "render/part_face_drawers.h"
#include "runtime/toystory2_context.h"

#include <cstdlib>
#include <lucent/log.h>
#include <memory>

namespace ts2 {
namespace {

template <std::uint32_t Renderer> void renderActor(Core *core) {
  ToyStory2Context &game = context(*core);
  game.actorProducers.setDrawing(game.actorIncarnations.generation(core->r[4]));
  psx::cpu::callOriginalToReturn(*core, Renderer, psx::cpu::ExecutionBudget::currentTurn(*core), "actor producer");
  game.actorProducers.setDrawing(std::nullopt);
}

} // namespace

void ActorProducers::install(Core &core) {
  psx::cpu::installNativeOverride(core, kActorRenderer, "actor-renderer", &renderActor<kActorRenderer>);
  psx::cpu::installNativeOverride(
      core, kScaledActorRenderer, "scaled-actor-renderer", &renderActor<kScaledActorRenderer>);
  PartFaceDrawers::install(core);
  core.stateProducers.install(kActorRenderer, std::make_unique<PartStateRender>(core));
}

std::uint32_t ActorProducers::partObject(std::uint32_t part, std::uint32_t generation) {
  if (part < kPartTable || (part - kPartTable) % kPartBytes != 0u) {
    lucent::error("ts2-actors", "0x{:08X} is not a record of the part table", part);
    std::abort();
  }
  return incarnationObject(part, generation);
}

} // namespace ts2
