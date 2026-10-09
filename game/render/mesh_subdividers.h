// The four near-camera subdividers the static submitter calls per visible primitive. Each is the guest subroutine
// run on the caller's registers: it returns v0 = 0 when the primitive is to be drawn whole, else the primitive was
// handled (drawn as sub-packets) or is too close to draw.
#pragma once

#include "render/mesh_cpu.h"

namespace ts2 {

void subdivideQuadTextured(MeshCpu &c);     // 0x80014D3C
void subdivideTriangleTextured(MeshCpu &c); // 0x80016690
void subdivideQuadPlain(MeshCpu &c);        // 0x8001A4D4
void subdivideTrianglePlain(MeshCpu &c);    // 0x8001B8AC

} // namespace ts2
