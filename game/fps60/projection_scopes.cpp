#include "fps60/projection_scopes.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "projection_provenance.h"
#include "runtime/toystory2_context.h"

#include <array>

namespace ts2::render {
namespace {

// A scope key: the producer's address in the high word, the instance below it. Guest RAM addresses fit
// in 21 bits, which leaves eight bits for the occurrence.
constexpr std::uint64_t kAddressMask = 0x1FFFFFu;
constexpr std::uint32_t kMaxOccurrence = 0xFFu;

std::uint64_t scopeKey(std::uint32_t producer, std::uint32_t instance, std::uint32_t occurrence) {
  return (static_cast<std::uint64_t>(producer) << 32) | ((instance & kAddressMask) << 8) | occurrence;
}

template <std::uint32_t Producer> void scopedSlotTableProducer(Core *core) {
  const psxport::temporal::ProjectionProvenance::Scope scope(
      core->rsub.projectionProvenance, ResidentProjectionScopes::slotTableInstance(*core, Producer));
  callOriginalToReturn(*core, Producer, "resident projection scope");
}

template <std::uint32_t Producer> void scopedProducer(Core *core) {
  const std::uint64_t key = context(*core).projectionScopes.producerInstance(Producer, core->r[4]);
  const psxport::temporal::ProjectionProvenance::Scope scope(core->rsub.projectionProvenance, key);
  callOriginalToReturn(*core, Producer, "resident projection scope");
}

// A producer the guest calls with no arguments, so nothing in the registers identifies WHICH call
// this is: its occurrence within the field is the whole identity, exactly as it is for the modelled
// producers above.
void scopedVisibilityPass(Core *core) {
  const std::uint64_t key = context(*core).projectionScopes.passInstance(ResidentProjectionScopes::kVisibilityPass);
  const psxport::temporal::ProjectionProvenance::Scope scope(core->rsub.projectionProvenance, key);
  callOriginalToReturn(*core, ResidentProjectionScopes::kVisibilityPass, "resident projection scope");
}

struct ScopedProducer {
  std::uint32_t address;
  const char *name;
  NativeGuestFunction function;
};

constexpr std::array kScopedProducers{
    ScopedProducer{ResidentProjectionScopes::kRigidMeshDrawer,
                   "projection-scope-rigid-mesh",
                   scopedSlotTableProducer<ResidentProjectionScopes::kRigidMeshDrawer>},
    ScopedProducer{0x8002518Cu, "projection-scope-object-renderer", scopedProducer<0x8002518Cu>},
    ScopedProducer{0x8002AC40u, "projection-scope-model-8002AC40", scopedProducer<0x8002AC40u>},
    ScopedProducer{0x8002B1D4u, "projection-scope-model-8002B1D4", scopedProducer<0x8002B1D4u>},
    ScopedProducer{0x8002B6F0u, "projection-scope-model-8002B6F0", scopedProducer<0x8002B6F0u>},
    ScopedProducer{0x8002C278u, "projection-scope-model-8002C278", scopedProducer<0x8002C278u>},
    ScopedProducer{ResidentProjectionScopes::kVisibilityPass, "projection-scope-visibility-pass", scopedVisibilityPass},
};

} // namespace

void ResidentProjectionScopes::beginFrame() {
  occurrences_.clear();
}

std::uint64_t ResidentProjectionScopes::slotTableInstance(Core &core, std::uint32_t producer) {
  return scopeKey(producer, core.mem_r32(kInstanceSlotTable), 0);
}

std::uint64_t ResidentProjectionScopes::producerInstance(std::uint32_t producer, std::uint32_t argument) {
  std::uint32_t &seen = occurrences_[scopeKey(producer, argument, 0)];
  if (seen > kMaxOccurrence) {
    return 0; // more instances of one model than a key can tell apart: record nothing for them
  }
  return scopeKey(producer, argument, seen++);
}

std::uint64_t ResidentProjectionScopes::passInstance(std::uint32_t producer) {
  return producerInstance(producer, 0);
}

void installResidentProjectionScopes(Core &core) {
  for (const ScopedProducer &producer : kScopedProducers) {
    installResidentOverride(core, producer.address, producer.name, producer.function);
  }
}

} // namespace ts2::render
