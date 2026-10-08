#include "cd/file_transfer.h"

#include "core.h"
#include "disc.h"
#include "execution/guest_execution.h"
#include "game.h"
#include "invalidation.h"
#include "native_dispatch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <lucent/log.h>
#include <string>
#include <vector>

namespace ts2::cd {
namespace {

// The guest's whole-file read at 0x80082608. It clears the two transfer words, publishes CD mode
// through 0x80090D40 with (0xB, 0, 0), and returns the file size, 1 for empty, or 0 on failure.
constexpr std::uint32_t kWholeFileRead = 0x80082608u;
constexpr std::uint32_t kCdModeLeaf = 0x80090D40u;
constexpr std::uint32_t kCdModeArguments = 0x0Bu;
constexpr std::uint32_t kTransferWordLo = 0x800A1034u;
constexpr std::uint32_t kTransferWordHi = 0x800A1588u;

// The guest RAM window a file load may fill; the executable and BSS end at 0x800D12C0+0x126D40.
constexpr std::uint32_t kGuestRamBase = 0x80000000u;
constexpr std::uint32_t kGuestRamEnd = 0x80200000u;
constexpr std::uint32_t kSectorBytes = 2048u;
// Guest paths are bounded ISO9660 names.
constexpr std::size_t kMaxGuestPath = 128u;

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

constexpr std::uint32_t kLoadFile = 0x80082728u;
constexpr std::uint32_t kRetryFlag = 0x800A15A8u;
// The guest repeated only while a transfer was unfinished; this one is synchronous, so a few attempts
// cover a transient read error without turning failure into a stall.
constexpr unsigned kMaxAttempts = 3u;

void loadFileOverride(Core *core) {
  const std::uint32_t path = core->r[4];
  const std::uint32_t destination = core->r[5];
  const std::array<std::uint32_t, 2> readArguments{path, destination};
  core->mem_w32(kRetryFlag, 0);
  for (unsigned attempt = 1; attempt <= kMaxAttempts; ++attempt) {
    // Dispatched through the guest so 0x80082608 stays the only path that touches the disc.
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
  const std::string path = guestString(core, guestPath, kMaxGuestPath);
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
  // One notification for the whole file; per-byte would invalidate translated code once per byte.
  if (fileBytes != 0) {
    psx::cpu::notifyExecutableWrite(core,
                                    {destination & 0x1FFFFFFFu, (destination & 0x1FFFFFFFu) + fileBytes},
                                    psx::cpu::ExecutableWriteSource::ModuleLoad);
  }
  outcome.transferred = true;
  outcome.bytes = fileBytes == 0 ? 1u : fileBytes;
  return outcome;
}

void FileTransfer::install(Core &core) {
  psx::cpu::installNativeOverride(core, kWholeFileRead, "cd-file-transfer", fileTransferOverride);
  psx::cpu::installNativeOverride(core, kLoadFile, "cd-load-file", loadFileOverride);
}

} // namespace ts2::cd
