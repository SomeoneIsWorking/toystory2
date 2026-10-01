#include "cd/file_transfer.h"

#include "core.h"
#include "core/guest_execution.h"
#include "disc.h"
#include "game.h"
#include "invalidation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <lucent/log.h>
#include <string>
#include <vector>

namespace ts2::cd {
namespace {

// The guest's own whole-file read, DECOMPILED WHOLE (Ghidra, exact bytes, 72 instructions at
// 0x80082608). Its four measured facts, each one the product depends on:
//   * the two words it clears on entry, DAT_800A1034 and DAT_800A1588, and
//   * the CD-mode publication it makes through 0x80090D40 with (0xB, 0, 0) BEFORE it reads anything,
//     which the parts of this product that still stream through the guest's CD depend on, and
//   * the size it returns: the file's size in bytes, or 1 for a file the TOC reports as empty, and
//   * 0 when the read failed.
constexpr std::uint32_t kWholeFileRead = 0x80082608u;
constexpr std::uint32_t kCdModeLeaf = 0x80090D40u;
constexpr std::uint32_t kCdModeArguments = 0x0Bu;
constexpr std::uint32_t kTransferWordLo = 0x800A1034u;
constexpr std::uint32_t kTransferWordHi = 0x800A1588u;

// The guest RAM window this title owns, and the only destination a file load may fill. The
// executable plus its BSS ends at 0x800D12C0 and the BSS runs to 0x800D12C0+0x126D40, so 2 MiB is the
// whole addressable guest space and a load outside it is a request this owner refuses rather than a
// transfer it attempts.
constexpr std::uint32_t kGuestRamBase = 0x80000000u;
constexpr std::uint32_t kGuestRamEnd = 0x80200000u;
constexpr std::uint32_t kSectorBytes = 2048u;
// A guest path is a bounded ISO9660 name; the loader's own normalizer upper-cases it and strips the
// version suffix, and a longer string than this is not a path this title ever builds.
constexpr std::size_t kMaxGuestPath = 128u;

std::string guestPathText(Core &core, std::uint32_t guestPath) {
  std::string text;
  for (std::size_t offset = 0; offset < kMaxGuestPath; ++offset) {
    const char c = static_cast<char>(core.mem_r8(guestPath + static_cast<std::uint32_t>(offset)));
    if (c == 0) {
      return text;
    }
    text.push_back(c);
  }
  return std::string();
}

void publishGuestTransferState(Core &core) {
  core.mem_w32(kTransferWordLo, 0);
  core.mem_w32(kTransferWordHi, 0);
}

void publishCdMode(Core &core) {
  const std::array arguments{kCdModeArguments, 0u, 0u, 0u};
  callGuestToReturn(core, {kCdModeLeaf, 0x80082618u, arguments, std::nullopt, "Toy Story 2 CD mode"});
}

void fileTransferOverride(Core *core) {
  FileTransfer transfer;
  const FileTransfer::Outcome outcome = transfer.transfer(*core, core->r[4], core->r[5]);
  if (!outcome.transferred) {
    lucent::error("ts2-file", "refused 0x{:08X} -> 0x{:08X}: {}", core->r[4], core->r[5], outcome.why);
  }
  core->r[2] = outcome.bytes;
}

// `0x80082728(path, destination)`, the loader's own retry policy, DECOMPILED WHOLE (Ghidra, exact
// bytes, 26 instructions at 0x80082728):
//
//     DAT_800A15A8 = 0;
//     do { do { iVar1 = 0x80082608(path, destination); } while (iVar1 < 1); }
//        while (DAT_800A15A8 == 1);
//     return iVar1;
//
// Both loops are unbounded and their only exit is a successful read, so a request this product cannot
// honour would spin the guest forever with no picture to show for it. The bounded form below keeps the
// guest's own contract — the retry word is cleared first, a successful read's size comes back, and the
// inner loop stops as soon as a read succeeds — and adds the two things the guest lacked: a bound, and
// a refusal that says why.
constexpr std::uint32_t kLoadFile = 0x80082728u;
constexpr std::uint32_t kRetryFlag = 0x800A15A8u;
// Three attempts, because the only reason the guest's own loop ever repeated was a transfer that had
// not finished, and this transfer is synchronous: it either produced the file or it did not. Three
// tries cover a transient read error without turning a failure into a stall.
constexpr unsigned kMaxAttempts = 3u;

void loadFileOverride(Core *core) {
  const std::uint32_t path = core->r[4];
  const std::uint32_t destination = core->r[5];
  const std::array<std::uint32_t, 2> readArguments{path, destination};
  core->mem_w32(kRetryFlag, 0);
  for (unsigned attempt = 1; attempt <= kMaxAttempts; ++attempt) {
    // The read is dispatched through the guest's own call so that the transfer owner at 0x80082608
    // stays the one thing on this path that touches the disc.
    const std::uint32_t bytes = callGuestToReturn(
        *core, {kWholeFileRead, kLoadFile + 0x28u, readArguments, std::nullopt, "Toy Story 2 file transfer"});
    if (bytes >= 1u) {
      core->r[2] = bytes;
      return;
    }
    if (core->mem_r32(kRetryFlag) != 1u) {
      break;
    }
    lucent::warn("ts2-file", "transfer attempt {} of {} read nothing for 0x{:08X}", attempt, kMaxAttempts, path);
  }
  lucent::error(
      "ts2-file", "gave up on 0x{:08X} -> 0x{:08X} after at most {} attempts", path, destination, kMaxAttempts);
  core->r[2] = 0;
}

} // namespace

FileTransfer::Outcome FileTransfer::transfer(Core &core, std::uint32_t guestPath, std::uint32_t destination) const {
  Outcome outcome;
  if (guestPath < kGuestRamBase || guestPath >= kGuestRamEnd) {
    outcome.why = "the path pointer is not guest RAM";
    return outcome;
  }
  if (destination < kGuestRamBase || destination >= kGuestRamEnd) {
    outcome.why = "the destination is not guest RAM";
    return outcome;
  }
  const std::string path = guestPathText(core, guestPath);
  if (path.empty()) {
    outcome.why = "the path is empty or longer than a disc name can be";
    return outcome;
  }

  std::uint32_t firstSector = 0;
  std::uint32_t fileBytes = 0;
  if (!disc_find_file(&core.game->disc, path.c_str(), &firstSector, &fileBytes)) {
    outcome.why = "the disc has no such file";
    return outcome;
  }
  if (fileBytes > kGuestRamEnd - destination) {
    outcome.why = "the file does not fit in guest RAM at that destination";
    return outcome;
  }

  publishGuestTransferState(core);
  publishCdMode(core);

  std::array<std::uint8_t, kSectorBytes> sector{};
  const std::uint32_t sectors = (fileBytes + kSectorBytes - 1u) / kSectorBytes;
  for (std::uint32_t index = 0; index < sectors; ++index) {
    if (!disc_read_sector(&core.game->disc, firstSector + index, sector.data())) {
      outcome.why = "a sector of the file could not be read";
      return outcome;
    }
    const std::uint32_t offset = index * kSectorBytes;
    const std::uint32_t count = std::min<std::uint32_t>(kSectorBytes, fileBytes - offset);
    for (std::uint32_t byte = 0; byte < count; ++byte) {
      core.mem_w8_unnotified(destination + offset + byte, sector[byte]);
    }
  }
  // The whole file is ONE transfer, so the executable-write notification is ONE range covering it,
  // exactly as the framework's own CD loadfile publishes its burst. A notification per byte would
  // invalidate translated code over a module once per byte of that module.
  if (fileBytes != 0) {
    psx::cpu::notifyExecutableWrite(core,
                                    {destination & 0x1FFFFFFFu, (destination & 0x1FFFFFFFu) + fileBytes},
                                    psx::cpu::ExecutableWriteSource::ModuleLoad);
  }
  outcome.transferred = true;
  outcome.bytes = fileBytes == 0 ? 1u : fileBytes;
  return outcome;
}

void installFileTransferOverride(Core &core) {
  installResidentOverride(core, kWholeFileRead, "cd-file-transfer", fileTransferOverride);
  installResidentOverride(core, kLoadFile, "cd-load-file", loadFileOverride);
}

} // namespace ts2::cd
