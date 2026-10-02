// resident_projection_scopes.h — which guest call is which producer instance, for 60 fps provenance.
//
// The framework pairs a drawn vertex with the same vertex in the previous real frame by (scope, epoch,
// model-space vertex) — see psxport's projection_provenance.h. The SCOPE is the one thing it cannot know:
// that a given guest call submits one particular object. This owner supplies it for the resident
// producers, each keyed by what the guest itself uses to tell its instances apart:
//
//   0x800100E4  static mesh submitter      the instance's per-face packet-slot table (0x800A11CC,
//   0x80017FF8  rigid mesh drawer of the   re-pointed by the caller for every instance it submits:
//               second submitter 0x80026D34 0x8002622C cases 9/C and 0x80026D34 cases 1/9 alike)
//   0x8002518C  animated object renderer   a0, the object record (position, parts, animation)
//   0x8002AC40, 0x8002B1D4, 0x8002B6F0,    a0, the model, plus its occurrence in this frame: these are
//   0x8002C278  hierarchical model drawers reached from the scene owner's cases 2/3/10 with the model
//                                          only, so two instances of one model differ by order alone
//
// A key is never reused for a different instance within a frame, and a producer that is not scoped here
// records nothing, so its primitives are presented at their real positions in the in-between.
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

class Core;

namespace ts2::render {

class ResidentProjectionScopes {
public:
  // The two producers that write an instance's faces into its own persistent packet slots, and the
  // guest's per-instance slot-table pointer their callers publish before each call.
  static constexpr std::uint32_t kMeshSubmitter = 0x800100E4u;
  static constexpr std::uint32_t kRigidMeshDrawer = 0x80017FF8u;
  static constexpr std::uint32_t kInstanceSlotTable = 0x800A11CCu;

  // A resident update is about to run: occurrence counts restart.
  void beginFrame();

  // The scope of a slot-table producer call about to run.
  static std::uint64_t slotTableInstance(Core &core, std::uint32_t producer);
  // The scope of a producer call keyed by its first argument and its occurrence in this frame.
  std::uint64_t producerInstance(std::uint32_t producer, std::uint32_t argument);

private:
  std::unordered_map<std::uint64_t, std::uint32_t> occurrences_;
};

// Install scope-opening observers on the producers above other than 0x800100E4, whose override belongs
// to the resident scene observer (resident_scene_history.cpp), which opens the mesh scope itself.
void installResidentProjectionScopes(Core &core);

} // namespace ts2::render
