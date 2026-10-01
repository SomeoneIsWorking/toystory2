// resident_view_matrix.h — the camera the GUEST publishes, as the renderer's view transform.
//
// 60fps interpolation needs a view for a time between two display fields, and the guest's producer
// (retail `0x8002C848`) only ever composes one per field. This does not re-own that producer: a full
// transcription of it was measured against the guest with the framework's per-call override
// differential and still published a camera that differs from the guest's own, which
// `docs/re-frontier.md` RE-05a records with the numbers.
//
// What this reads instead is the camera the guest has ALREADY published for the current field, at the
// GTE addresses retail `0x8002C848` fills every tick. That is the guest's own view, byte for byte,
// with nothing inferred: the same sixteen words the guest's own world pass is built from.
#pragma once

#include <cstdint>

class Core;

namespace ts2::render {

// Fill `view` with the guest's published camera rotation and `translation` with its published
// position, both in the guest's own GTE units: the rotation elements are 1.3.12 counts and the
// position is already shifted into the GTE's sub-word units, which is the space the guest projects
// its world in.
void readResidentView(Core &core, float view[3][3], float translation[3]);

} // namespace ts2::render