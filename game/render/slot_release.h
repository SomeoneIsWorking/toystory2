// The submitters' release of a subdivided packet: its children's slots go back on the released list and the
// subtree's flags are cleared in both packet arrays.
#pragma once

#include "render/mesh_cpu.h"

namespace ts2 {

// Child slot ids sit in two words of the parent: the first at +8, the second at +20 in the 12-word textured
// packets (0x80017B34) and at +16 in the plain ones (0x80017ECC).
enum class PacketKind { Textured, Plain };

// The guest subroutine on the caller's registers: a3 is the packet and ra the call site's return address.
// Leaves v0 (the other array), v1 (the released cursor, also stored back), a3 and at as the guest does.
void releaseSubtree(MeshCpu &c, PacketKind kind);

} // namespace ts2
