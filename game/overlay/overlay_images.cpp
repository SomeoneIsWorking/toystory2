#include "overlay/overlay_images.h"

#include "core.h"
#include "disc.h"
#include "fmv/guest_movie_player.h"
#include "game.h"
#include "guest_execution.h"
#include "toystory2_context.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <lucent/log.h>
#include <vector>

namespace ts2 {
namespace {

constexpr std::uint32_t kFileLoader = 0x80082508u;
constexpr std::uint32_t kSectorBytes = 2048u;

std::vector<std::uint8_t> readDiscImage(Core &core, const OverlayModule &module) {
  std::uint32_t firstSector = 0;
  std::uint32_t fileBytes = 0;
  const std::string path{module.discPath};
  if (!disc_find_file(&core.game->disc, path.c_str(), &firstSector, &fileBytes) || fileBytes != module.fileBytes) {
    lucent::error(
        "ts2-overlay", "{} lookup refused: expected {} bytes at {}", module.identityName, module.fileBytes, path);
    std::abort();
  }

  std::vector<std::uint8_t> bytes(fileBytes);
  std::array<std::uint8_t, kSectorBytes> sector{};
  for (std::uint32_t offset = 0; offset < fileBytes; offset += kSectorBytes) {
    if (!disc_read_sector(&core.game->disc, firstSector + offset / kSectorBytes, sector.data())) {
      lucent::error(
          "ts2-overlay", "{} sector {} could not be read", module.identityName, firstSector + offset / kSectorBytes);
      std::abort();
    }
    const std::size_t count = std::min<std::size_t>(sector.size(), fileBytes - offset);
    std::copy_n(sector.begin(), count, bytes.begin() + offset);
  }
  return bytes;
}

std::string guestPathText(Core &core, std::uint32_t guestPath) {
  std::string text;
  for (std::uint32_t offset = 0; guestPath != 0 && offset < 64u; ++offset) {
    const char c = static_cast<char>(core.mem_r8(guestPath + offset));
    if (c == 0) {
      break;
    }
    text.push_back(c);
  }
  return text;
}

void observeFileLoad(Core *core) {
  const std::uint32_t guestPath = core->r[4];
  const std::uint32_t destination = core->r[5];
  OverlaySlot *slot = context(*core).overlays.slotAt(destination);
  if (slot == nullptr) {
    callOriginalToReturn(*core, kFileLoader, "Toy Story 2 file loader original");
    return;
  }

  const auto module = slot->matchLoad(*core, guestPath, destination);
  std::vector<std::uint8_t> discBytes;
  std::string why;
  if (module) {
    discBytes = readDiscImage(*core, slot->module(*module));
    if (!slot->authenticateSource(*module, discBytes, why)) {
      lucent::error(
          "ts2-overlay", "{} disc image refused before guest transfer: {}", slot->module(*module).identityName, why);
      std::abort();
    }
  }
  callOriginalToReturn(*core, kFileLoader, "Toy Story 2 file loader original");
  if (!module) {
    // Whatever the guest loaded here is not an authenticated code module (the LEVEL00 placeholder is
    // data), so nothing may stay executable from the previous contents.
    lucent::info("ts2-overlay",
                 "slot 0x{:08X} loaded non-module '{}'; identity retired",
                 destination,
                 guestPathText(*core, guestPath));
    slot->retire(*core);
    return;
  }
  if (!slot->publish(*core, *module, discBytes, why)) {
    lucent::error("ts2-overlay", "{} image publication refused: {}", slot->module(*module).identityName, why);
    std::abort();
  }
  // The FMV module is one whose published contents the port replaces with a native owner: its movie
  // player streams sectors through a CD path this port serves instantly, which floods the audio ring
  // and stalls the disc mid-movie. Publishing it is what makes the player entry addressable as that
  // module rather than as whatever else shares 0x800D5D20, so the native player is installed here,
  // scoped to the identity just published. No other module in the slot has one.
  if (slot->module(*module).identityName == SharedSlotImage::kFmvIdentityName) {
    fmv::installGuestMoviePlayer(*core);
  }
  const auto identity = slot->activeIdentity();
  lucent::info("ts2-overlay",
               "authenticated {} image {}:{} at 0x{:08X}, {} bytes",
               slot->module(*module).identityName,
               identity->id,
               identity->generation,
               destination,
               discBytes.size());
}

} // namespace

OverlaySlot *OverlayImages::slotAt(std::uint32_t destination) {
  for (OverlaySlot *slot : {&level_, &shared_}) {
    if (slot->loadAddress() == destination) {
      return slot;
    }
  }
  return nullptr;
}

void installOverlayLoadObserver(Core &core) {
  installResidentOverride(core, kFileLoader, "overlay-image-loader", observeFileLoad);
}

} // namespace ts2
