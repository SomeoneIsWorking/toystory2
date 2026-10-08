#include "render/actor_incarnation.h"

#include "core.h"
#include "execution_exit.h"
#include "native_dispatch.h"
#include "runtime/toystory2_context.h"

namespace ts2 {
namespace {

void actorReset(Core *core) {
  const std::uint32_t actor = core->r[4];
  psx::cpu::callOriginalToReturn(
      *core, ActorIncarnations::kActorReset, psx::cpu::ExecutionBudget::currentTurn(*core), "actor reset");
  context(*core).actorIncarnations.begin(actor);
}

} // namespace

void ActorIncarnations::install(Core &core) {
  psx::cpu::installNativeOverride(core, kActorReset, "actor-reset", &actorReset);
}

void ActorIncarnations::begin(std::uint32_t actor) {
  ++generations_[actor & kActorOffsetMask];
}

std::uint32_t ActorIncarnations::object(std::uint32_t actor) const {
  const auto found = generations_.find(actor & kActorOffsetMask);
  return incarnationObject(actor, found == generations_.end() ? 0u : found->second);
}

} // namespace ts2
