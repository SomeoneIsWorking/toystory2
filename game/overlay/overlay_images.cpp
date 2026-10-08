#include "overlay/overlay_images.h"

#include "core.h"
#include "disc.h"
#include "execution/guest_execution.h"
#include "execution_exit.h"
#include "fmv/movie_player.h"
#include "game.h"
#include "native_dispatch.h"
#include "runtime/toystory2_context.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <lucent/log.h>
#include <vector>

namespace ts2 {
namespace {

constexpr std::uint32_t kFileLoader = 0x80082508u;
constexpr std::uint32_t kSectorBytes = 2048u;
constexpr std::size_t kMaxGuestPath = 128u;

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

void observeFileLoad(Core *core) {
  const std::uint32_t guestPath = core->r[4];
  const std::uint32_t destination = core->r[5];
  OverlaySlot *slot = context(*core).overlays.slotAt(destination);
  if (slot == nullptr) {
    psx::cpu::callOriginalToReturn(
        *core, kFileLoader, psx::cpu::ExecutionBudget::currentTurn(*core), "Toy Story 2 file loader original");
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
  psx::cpu::callOriginalToReturn(
      *core, kFileLoader, psx::cpu::ExecutionBudget::currentTurn(*core), "Toy Story 2 file loader original");
  if (!module) {
    // A non-module load (the LEVEL00 placeholder is data) must not leave old code executable.
    lucent::info("ts2-overlay",
                 "slot 0x{:08X} loaded non-module '{}'; identity retired",
                 destination,
                 guestString(*core, guestPath, kMaxGuestPath));
    slot->retire(*core);
    return;
  }
  if (!slot->publish(*core, *module, discBytes, why)) {
    lucent::error("ts2-overlay", "{} image publication refused: {}", slot->module(*module).identityName, why);
    std::abort();
  }
  // The FMV module gets a native movie player: the retail one streams CD sectors that this port serves
  // instantly, flooding the audio ring. Scoped to the identity just published, since 0x800D5D20 is shared.
  if (slot->module(*module).identityName == SharedSlotImage::kFmvIdentityName) {
    context(*core).moviePlayer.install(*core);
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

void OverlayImages::installLoadObserver(Core &core) {
  psx::cpu::installNativeOverride(core, kFileLoader, "overlay-image-loader", observeFileLoad);
}

} // namespace ts2
