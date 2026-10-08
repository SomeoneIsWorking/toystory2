#pragma once

class Core;

namespace ts2::fmv {

// The guest's player outruns the XA ring and stalls under instant CD; this supplies only which movie
// was asked for and what the retail player returns.
class GuestMoviePlayer {
public:
  // 0x800D5D20 is a slot MEMORY.BIN also occupies, so install only once the FMV image is resident.
  void install(Core &core);
};

} // namespace ts2::fmv
