// The near-camera triangle subdividers' split path (0x8001673C textured, 0x8001B958 plain): a triangle too
// close to draw whole is cut into four quarters or, with some edges short, a fan of three around a point on its
// median; each child is linked, culled or cut again.
#pragma once

#include "render/mesh_cpu.h"
#include "render/slot_release.h"

namespace ts2 {

// Runs the guest's split on the subdivider's registers, with the frame in t9. Leaves v0 as the guest does: 1 once
// the children are handled, else the short-edge mask when the free list had no children to give.
void splitTriangle(MeshCpu &c, PacketKind kind);

} // namespace ts2
