// How a quad or triangle is cut into children, and whether the children are new.
#pragma once

namespace ts2 {

// Quarters: each child is one fourth of the parent. Fan: each child lies on one edge and reaches the centre.
enum class SplitLayout { Quarters, Fan };

// Fresh children are written to both packet arrays; adopted ones, left over from an earlier split, only to the
// one drawn from.
enum class SplitOrigin { Fresh, Adopted };

} // namespace ts2
