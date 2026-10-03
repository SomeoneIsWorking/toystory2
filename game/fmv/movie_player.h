#pragma once

class Core;

namespace ts2::fmv {

// The native movie player for the guest's FMV overlay. The FMV module's own player streams a .STR
// through the guest's CD path, which this port serves instantly, so it outruns the drive, floods the
// XA ring and stalls the disc mid-movie. This owner plays the movie through psxport's own player and
// supplies only what the guest knows: WHICH movie was asked for, and WHAT the retail player returns.
class GuestMoviePlayer {
public:
  // Install the movie-player override for the FMV image generation, once that generation is resident.
  // 0x800D5D20 is a shared slot MEMORY.BIN also occupies, so an address there means nothing without
  // the module identity.
  void install(Core &core);
};

} // namespace ts2::fmv
