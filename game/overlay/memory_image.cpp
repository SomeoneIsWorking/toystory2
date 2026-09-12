#include "overlay/memory_image.h"

#include "core.h"
#include "disc.h"
#include "game.h"
#include "guest_execution.h"
#include "invalidation.h"
#include "lightrec_executor.h"
#include "toystory2_context.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <lucent/content.h>
#include <lucent/log.h>
#include <vector>

namespace ts2 {
namespace {

constexpr std::uint32_t kFileLoader = 0x80082508u;
constexpr std::uint32_t kSectorBytes = 2048u;
constexpr std::uint32_t kPhysicalLoadAddress = MemoryOverlayImage::kLoadAddress & 0x1FFFFFFFu;
constexpr GuestAddressRange kMemoryRange{kPhysicalLoadAddress, kPhysicalLoadAddress + MemoryOverlayImage::kFileBytes};

std::uint8_t upperAscii(std::uint8_t value) {
  return value >= 'a' && value <= 'z' ? static_cast<std::uint8_t>(value - ('a' - 'A')) : value;
}

std::vector<std::uint8_t> readDiscImage(Core &core) {
  std::uint32_t firstSector = 0;
  std::uint32_t fileBytes = 0;
  const std::string path{MemoryOverlayImage::kDiscPath};
  if (!disc_find_file(&core.game->disc, path.c_str(), &firstSector, &fileBytes) ||
      fileBytes != MemoryOverlayImage::kFileBytes) {
    lucent::error(
        "ts2-overlay", "MEMORY image lookup refused: expected {} bytes at {}", MemoryOverlayImage::kFileBytes, path);
    std::abort();
  }

  std::vector<std::uint8_t> bytes(fileBytes);
  std::array<std::uint8_t, kSectorBytes> sector{};
  for (std::uint32_t offset = 0; offset < fileBytes; offset += kSectorBytes) {
    if (!disc_read_sector(&core.game->disc, firstSector + offset / kSectorBytes, sector.data())) {
      lucent::error("ts2-overlay", "MEMORY image sector {} could not be read", firstSector + offset / kSectorBytes);
      std::abort();
    }
    const std::size_t count = std::min<std::size_t>(sector.size(), fileBytes - offset);
    std::copy_n(sector.begin(), count, bytes.begin() + offset);
  }
  return bytes;
}

void observeFileLoad(Core *core) {
  const std::uint32_t guestPath = core->r[4];
  const std::uint32_t destination = core->r[5];
  const bool memoryImage = MemoryOverlayImage::matchesLoad(*core, guestPath, destination);
  std::vector<std::uint8_t> discBytes;
  std::string why;
  if (memoryImage) {
    discBytes = readDiscImage(*core);
    if (!context(*core).memoryImage.authenticateSource(discBytes, why)) {
      lucent::error("ts2-overlay", "MEMORY disc image refused before guest transfer: {}", why);
      std::abort();
    }
  }
  callOriginalToReturn(*core, kFileLoader, "Toy Story 2 file loader original");
  if (destination == MemoryOverlayImage::kLoadAddress && !memoryImage) {
    // FMV and MEMORY share this slot. A later load cannot leave MEMORY's old identity active.
    context(*core).memoryImage.retire(*core);
  }
  if (!memoryImage) {
    return;
  }

  if (!context(*core).memoryImage.publish(*core, discBytes, why)) {
    lucent::error("ts2-overlay", "MEMORY image publication refused: {}", why);
    std::abort();
  }
  const auto identity = context(*core).memoryImage.activeIdentity();
  lucent::info("ts2-overlay",
               "authenticated MEMORY image {}:{} at 0x{:08X}, {} bytes",
               identity->id,
               identity->generation,
               MemoryOverlayImage::kLoadAddress,
               discBytes.size());
}

} // namespace

bool MemoryOverlayImage::matchesLoad(Core &core, std::uint32_t guestPath, std::uint32_t destination) {
  if (destination != kLoadAddress || guestPath == 0) {
    return false;
  }
  for (std::size_t index = 0; index < kGuestPath.size(); ++index) {
    if (upperAscii(core.mem_r8(guestPath + static_cast<std::uint32_t>(index))) !=
        upperAscii(static_cast<std::uint8_t>(kGuestPath[index]))) {
      return false;
    }
  }
  return core.mem_r8(guestPath + static_cast<std::uint32_t>(kGuestPath.size())) == 0;
}

std::optional<lucent::content::Sha256> MemoryOverlayImage::authenticateSource(std::span<const std::uint8_t> discBytes,
                                                                              std::string &why) const {
  if (discBytes.size() != kFileBytes) {
    why = "disc file length differs from the measured MEMORY module";
    return std::nullopt;
  }
  const auto digest = lucent::content::sha256(std::as_bytes(discBytes));
  if (lucent::content::sha256_hex(digest) != expectedSha256_) {
    why = "disc file SHA-256 differs from the authenticated retail MEMORY module";
    return std::nullopt;
  }
  why.clear();
  return digest;
}

void MemoryOverlayImage::retire(Core &core) {
  if (active_) {
    core.imageCatalog().deactivate(*active_);
    active_.reset();
  }
  psx::cpu::notifyExecutableWrite(core, kMemoryRange, psx::cpu::ExecutableWriteSource::ModuleLoad);
}

bool MemoryOverlayImage::publish(Core &core, std::span<const std::uint8_t> discBytes, std::string &why) {
  const auto digest = authenticateSource(discBytes, why);
  if (!digest) {
    return false;
  }

  for (std::uint32_t index = 0; index < kFileBytes; ++index) {
    const std::uint8_t byte = discBytes[index];
    if (core.mem_r8(kLoadAddress + index) != byte) {
      retire(core);
      why = "transferred MEMORY bytes differ from the disc image at offset " + std::to_string(index);
      return false;
    }
  }
  retire(core);
  std::uint64_t contentIdentity = 0;
  for (std::size_t index = 0; index < sizeof(contentIdentity); ++index) {
    contentIdentity = (contentIdentity << 8u) | (*digest)[index];
  }
  active_ = core.imageCatalog().activate("BITS/MEMORY.BIN", kMemoryRange, contentIdentity);
  why.clear();
  return true;
}

std::optional<psx::cpu::ImageIdentity> MemoryOverlayImage::activeIdentity() const {
  return active_;
}

void installMemoryOverlayObserver(Core &core) {
  installResidentOverride(core, kFileLoader, "memory-image-loader", observeFileLoad);
}

} // namespace ts2
