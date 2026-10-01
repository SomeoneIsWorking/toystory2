// file_transfer.h — the whole-file read the guest's loader blocks on, owned natively.
#pragma once

#include <cstdint>
#include <string>

class Core;

namespace ts2::cd {

// The guest's own whole-file read, `0x80082608(path, destination)`, measured on SLUS_008.93:
//
//     DAT_800A1034 = 0; DAT_800A1588 = 0;
//     CdInit(0xB, 0, 0);                       // 0x80090D40
//     do { CdSearchFile(&file, path); } while (!ok);   // 0x80092AE8, an unbounded spin
//     if (file.size == 0) return 1;
//     sectors = (file.size + 0x7FF) >> 11;
//     CdRead(sectors, destination, 0x80);       // 0x80093AF0, one 128 KiB request
//     do { CdSync(1, 0); } while (busy);        // 0x80093BF4, the blocking whole-file wait
//     return file.size;
//
// Two of those four steps are walls the player waits through and neither of them is game state: the
// search spins until the drive answers, and the read spins until the transfer lands. This owner
// answers both from the disc image the title has ALREADY authenticated — the same `disc` owner
// `overlay/` reads its code modules from — so a file load costs one sector read per 2 KiB instead of
// a drive round trip, and the size the guest gets back is the size the disc file actually has,
// which is what its own CdlFILE would have carried.
//
// It REFUSES rather than transfers whenever the request is not one it can honour exactly: a path the
// disc does not have, a destination outside guest RAM, a file that would not fit in guest RAM, or a
// sector the disc cannot produce. A refusal transfers nothing and returns the guest's failure value,
// so no half-written buffer is ever presented as a loaded file.
class FileTransfer {
public:
  // One transfer's outcome. `bytes` is the size the guest is told about, which is the file's real
  // size on success and zero on refusal; `why` names the refusal and is empty on success.
  struct Outcome {
    bool transferred = false;
    std::uint32_t bytes = 0;
    std::string why;
  };

  // The disc files this owner may transfer, read through the authenticated disc image. `guestPath` is
  // the guest's own normalized spelling (upper case, ISO9660 version suffix already stripped by the
  // loader at 0x80082508), `destination` is a guest RAM address.
  Outcome transfer(Core &core, std::uint32_t guestPath, std::uint32_t destination) const;
};

// Its CALLER is the last unbounded wait in the load path, so this owner owns that too. `0x80082728`
// retries the whole-file read in two `do { ... } while` loops — an inner one that repeats while the
// read failed and an outer one that repeats while a guest word (`DAT_800A15A8`) says to — and its only
// exit is success. Both of the design's other loading-only waits sit inside that pair: the TOC search
// spin at `0x80082648` and the transfer sync spin at `0x8008276C`. Replacing the caller's retry policy,
// rather than the retry policy inside the read, keeps ONE owner of the disc read — this one — while
// making the retry bounded and observable: a read that can be honoured does so on its first attempt,
// and a read that cannot is reported as the guest's failure value instead of never returning.
//
// Install the native load path: the whole-file read at `0x80082608` and the bounded retry policy in
// its caller `0x80082728`. Registered only once the resident image is authenticated, alongside the
// other resident overrides, because it publishes nothing until it has been asked to.
void installFileTransferOverride(Core &core);

} // namespace ts2::cd
