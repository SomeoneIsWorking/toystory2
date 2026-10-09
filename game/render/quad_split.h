// The near-camera quad subdividers' split path (0x80014DF4 textured, 0x8001A58C plain): a quad too close to draw
// whole is cut into four children, either quarters or a fan around the centre, each linked, culled or cut again.
#pragma once

#include "render/mesh_cpu.h"
#include "render/slot_release.h"

namespace ts2 {

// Runs the guest's split on the subdivider's registers, with the frame in t9. Leaves v0 as the guest does: 1 once the
// children are handled, else the short-edge mask when the free list had no children to give.
void splitQuad(MeshCpu &c, PacketKind kind);

} // namespace ts2
