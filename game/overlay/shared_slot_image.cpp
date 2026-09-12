#include "overlay/shared_slot_image.h"

#include "core.h"
#include "disc.h"
#include "game.h"
#include "guest_execution.h"
#include "invalidation.h"
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
constexpr std::uint32_t kPhysicalLoadAddress = SharedSlotImage::kLoadAddress & 0x1FFFFFFFu;
constexpr GuestAddressRange kSharedSlotRange{
    kPhysicalLoadAddress,
    kPhysicalLoadAddress + std::max(SharedSlotImage::kMemoryFileBytes, SharedSlotImage::kFmvFileBytes)};

std::uint8_t upperAscii(std::uint8_t value) {
  return value >= 'a' && value <= 'z' ? static_cast<std::uint8_t>(value - ('a' - 'A')) : value;
}

bool guestPathMatches(Core &core, std::uint32_t guestPath, std::string_view expected) {
  for (std::size_t offset = 0; offset < expected.size(); ++offset) {
    if (upperAscii(core.mem_r8(guestPath + static_cast<std::uint32_t>(offset))) !=
        upperAscii(static_cast<std::uint8_t>(expected[offset]))) {
      return false;
    }
  }
  return core.mem_r8(guestPath + static_cast<std::uint32_t>(expected.size())) == 0;
}

std::vector<std::uint8_t> readDiscImage(Core &core, const SharedSlotImage::Spec &spec) {
  std::uint32_t firstSector = 0;
  std::uint32_t fileBytes = 0;
  const std::string path{spec.discPath};
  if (!disc_find_file(&core.game->disc, path.c_str(), &firstSector, &fileBytes) || fileBytes != spec.fileBytes) {
    lucent::error("ts2-overlay", "{} lookup refused: expected {} bytes at {}", spec.identityName, spec.fileBytes, path);
    std::abort();
  }

  std::vector<std::uint8_t> bytes(fileBytes);
  std::array<std::uint8_t, kSectorBytes> sector{};
  for (std::uint32_t offset = 0; offset < fileBytes; offset += kSectorBytes) {
    if (!disc_read_sector(&core.game->disc, firstSector + offset / kSectorBytes, sector.data())) {
      lucent::error(
          "ts2-overlay", "{} sector {} could not be read", spec.identityName, firstSector + offset / kSectorBytes);
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
  auto &image = context(*core).sharedSlotImage;
  const auto kind = SharedSlotImage::matchLoad(*core, guestPath, destination);
  std::vector<std::uint8_t> discBytes;
  std::string why;
  if (kind) {
    discBytes = readDiscImage(*core, SharedSlotImage::spec(*kind));
    if (!image.authenticateSource(*kind, discBytes, why)) {
      lucent::error("ts2-overlay",
                    "{} disc image refused before guest transfer: {}",
                    SharedSlotImage::spec(*kind).identityName,
                    why);
      std::abort();
    }
  }
  callOriginalToReturn(*core, kFileLoader, "Toy Story 2 file loader original");
  if (destination == SharedSlotImage::kLoadAddress && !kind) {
    image.retire(*core);
  }
  if (!kind) {
    return;
  }
  if (!image.publish(*core, *kind, discBytes, why)) {
    lucent::error("ts2-overlay", "{} image publication refused: {}", SharedSlotImage::spec(*kind).identityName, why);
    std::abort();
  }
  const auto identity = image.activeIdentity();
  lucent::info("ts2-overlay",
               "authenticated {} image {}:{} at 0x{:08X}, {} bytes",
               SharedSlotImage::spec(*kind).identityName,
               identity->id,
               identity->generation,
               SharedSlotImage::kLoadAddress,
               discBytes.size());
}

} // namespace

const SharedSlotImage::Spec &SharedSlotImage::spec(Kind kind) {
  static constexpr std::array<Spec, 2> specs{{
      {Kind::Memory, kMemoryGuestPath, "\\BITS\\MEMORY.BIN;1", "BITS/MEMORY.BIN", kMemoryFileBytes},
      {Kind::Fmv, kFmvGuestPath, "\\FMV\\FMV.BIN;1", "FMV/FMV.BIN", kFmvFileBytes},
  }};
  return specs.at(index(kind));
}

std::optional<SharedSlotImage::Kind>
SharedSlotImage::matchLoad(Core &core, std::uint32_t guestPath, std::uint32_t destination) {
  if (destination != kLoadAddress || guestPath == 0) {
    return std::nullopt;
  }
  for (Kind kind : {Kind::Memory, Kind::Fmv}) {
    if (guestPathMatches(core, guestPath, spec(kind).guestPath)) {
      return kind;
    }
  }
  return std::nullopt;
}

std::optional<lucent::content::Sha256>
SharedSlotImage::authenticateSource(Kind kind, std::span<const std::uint8_t> discBytes, std::string &why) const {
  const Spec &asset = spec(kind);
  if (discBytes.size() != asset.fileBytes) {
    why = "disc file length differs from the measured " + std::string(asset.identityName) + " module";
    return std::nullopt;
  }
  const auto digest = lucent::content::sha256(std::as_bytes(discBytes));
  if (lucent::content::sha256_hex(digest) != expectedSha256_.at(index(kind))) {
    why = "disc file SHA-256 differs from the authenticated retail " + std::string(asset.identityName) + " module";
    return std::nullopt;
  }
  why.clear();
  return digest;
}

void SharedSlotImage::retire(Core &core) {
  if (active_) {
    core.imageCatalog().deactivate(*active_);
    active_.reset();
  }
  psx::cpu::notifyExecutableWrite(core, kSharedSlotRange, psx::cpu::ExecutableWriteSource::ModuleLoad);
}

bool SharedSlotImage::publish(Core &core, Kind kind, std::span<const std::uint8_t> discBytes, std::string &why) {
  const auto digest = authenticateSource(kind, discBytes, why);
  if (!digest) {
    return false;
  }

  for (std::uint32_t offset = 0; offset < discBytes.size(); ++offset) {
    if (core.mem_r8(kLoadAddress + offset) != discBytes[offset]) {
      retire(core);
      why = "transferred " + std::string(spec(kind).identityName) + " bytes differ from the disc image at offset " +
            std::to_string(offset);
      return false;
    }
  }
  retire(core);
  std::uint64_t contentIdentity = 0;
  for (std::size_t offset = 0; offset < sizeof(contentIdentity); ++offset) {
    contentIdentity = (contentIdentity << 8u) | (*digest)[offset];
  }
  active_ = core.imageCatalog().activate(std::string(spec(kind).identityName),
                                         {kPhysicalLoadAddress, kPhysicalLoadAddress + spec(kind).fileBytes},
                                         contentIdentity);
  why.clear();
  return true;
}

std::optional<psx::cpu::ImageIdentity> SharedSlotImage::activeIdentity() const {
  return active_;
}

void installSharedSlotImageObserver(Core &core) {
  installResidentOverride(core, kFileLoader, "shared-slot-image-loader", observeFileLoad);
}

} // namespace ts2
