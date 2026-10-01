#include "overlay/overlay_slot.h"

#include "core.h"
#include "invalidation.h"

#include <cstring>

namespace ts2 {
namespace {

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

std::uint64_t contentIdentity(const lucent::content::Sha256 &digest) {
  std::uint64_t identity = 0;
  for (std::size_t offset = 0; offset < sizeof(identity); ++offset) {
    identity = (identity << 8u) | digest[offset];
  }
  return identity;
}

} // namespace

OverlaySlot::OverlaySlot(std::uint32_t loadAddress, std::uint32_t windowBytes, std::vector<OverlayModule> modules)
    : loadAddress_(loadAddress), windowBytes_(windowBytes), modules_(std::move(modules)) {}

std::optional<std::size_t>
OverlaySlot::matchLoad(Core &core, std::uint32_t guestPath, std::uint32_t destination) const {
  if (destination != loadAddress_ || guestPath == 0) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < modules_.size(); ++index) {
    if (guestPathMatches(core, guestPath, modules_[index].guestPath)) {
      return index;
    }
  }
  return std::nullopt;
}

std::optional<lucent::content::Sha256>
OverlaySlot::authenticateSource(std::size_t index, std::span<const std::uint8_t> discBytes, std::string &why) const {
  const OverlayModule &asset = modules_.at(index);
  if (discBytes.size() != asset.fileBytes) {
    why = "disc file length differs from the measured " + std::string(asset.identityName) + " module";
    return std::nullopt;
  }
  const auto digest = lucent::content::sha256(std::as_bytes(discBytes));
  if (lucent::content::sha256_hex(digest) != asset.retailSha256) {
    why = "disc file SHA-256 differs from the authenticated retail " + std::string(asset.identityName) + " module";
    return std::nullopt;
  }
  why.clear();
  return digest;
}

void OverlaySlot::retire(Core &core) {
  if (active_) {
    core.imageCatalog().deactivate(*active_);
    active_.reset();
  }
  const std::uint32_t physical = loadAddress_ & 0x1FFFFFFFu;
  psx::cpu::notifyExecutableWrite(
      core, GuestAddressRange{physical, physical + windowBytes_}, psx::cpu::ExecutableWriteSource::ModuleLoad);
}

bool OverlaySlot::publish(Core &core, std::size_t index, std::span<const std::uint8_t> discBytes, std::string &why) {
  const auto digest = authenticateSource(index, discBytes, why);
  if (!digest) {
    return false;
  }
  const OverlayModule &asset = modules_.at(index);
  if (asset.fileBytes > windowBytes_) {
    why = std::string(asset.identityName) + " does not fit the " + std::to_string(windowBytes_) + "-byte slot window";
    return false;
  }
  for (std::uint32_t offset = 0; offset < discBytes.size(); ++offset) {
    if (core.mem_r8(loadAddress_ + offset) != discBytes[offset]) {
      retire(core);
      why = "transferred " + std::string(asset.identityName) + " bytes differ from the disc image at offset " +
            std::to_string(offset);
      return false;
    }
  }
  retire(core);
  const std::uint32_t physical = loadAddress_ & 0x1FFFFFFFu;
  active_ = core.imageCatalog().activate(
      std::string(asset.identityName), {physical, physical + asset.fileBytes}, contentIdentity(*digest));
  why.clear();
  return true;
}

} // namespace ts2
