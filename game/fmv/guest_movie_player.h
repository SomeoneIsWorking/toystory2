#pragma once

class Core;

namespace ts2::fmv {

// The native movie player for the guest's FMV overlay.
//
// The FMV module's own player (`FUN_800D7088` in FMV/FMV.BIN) is a streaming loop: it asks the guest's
// CD path for the next sector of a .STR as it decodes, and waits on VSync once per movie frame. This
// port's CD path serves reads instantly, so that loop spends the movie pulling sectors far faster than
// a drive delivers them, floods the XA audio ring, and (through the drive's own backpressure) stalls
// the disc — which starves the VIDEO sectors of the same stream. The result presented as a mostly
// black frame with fragments, and the same movie also played its audio at the wrong rate.
//
// So the movie is played by psxport's one movie owner, `Game::fmv` (STR demux -> BS VLC -> MDEC ->
// present, XA audio on its own host stream, video paced to the media clock, Start skips), and this
// owner only supplies what only the guest knows: WHICH movie the caller asked for, and WHAT the
// retail player returns.
//
// Contract, from the retail bytes (`docs/re-frontier.md` RE-19):
//   * entry  `0x800D7088` — `(path, startLba, depth, unused, sectorCount, unused, arg)`.
//     `path` is a guest-RAM string like "toy2fmv\\dlogo.str".
//   * return 0 when the movie reached its end, and `_DAT_800A1670` when the guest asked to skip.
//     The caller (`FUN_800D6628`) returns that unchanged to the front-end sequencer, which treats
//     nonzero as "the cold intro is finished" — so the skip answer is what makes Start skip the
//     REMAINING movies as well as this one.
//   * The entry is identity-scoped: 0x800D5D20 is a shared slot that MEMORY.BIN also occupies, and
//     an address there means nothing without the module identity, so the override is installed for
//     the FMV image generation and only when that generation is resident.
void installGuestMoviePlayer(Core &core);

} // namespace ts2::fmv
