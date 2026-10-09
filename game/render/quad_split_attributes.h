// How a quad packet's colours and texture coordinates are divided over its four children.
#pragma once

#include "render/mesh_cpu.h"
#include "render/split_format.h"
#include "render/split_layout.h"

#include <span>

namespace ts2 {

// Writes the children's colour words and, for a textured packet, uv words, from the parent at `parent`. `primary`
// and `alt` are the children's packet addresses in the two arrays (`alt` empty for adopted children). Where the
// guest parks the shared values of the uv averaging in the scratchpad, so does this. v1 and at end as the
// guest leaves them.
void splitQuadAttributes(MeshCpu &c,
                         const SplitFormat &format,
                         SplitLayout layout,
                         SplitOrigin origin,
                         std::uint32_t parent,
                         std::span<const std::uint32_t> primary,
                         std::span<const std::uint32_t> alt);

} // namespace ts2
