#pragma once

#include "image_identity.h"

#include <lucent/content.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class Core;

namespace ts2 {

// One retail file the game's loader places in a fixed guest-RAM slot: its guest-side spelling, its
// disc location, and the exact bytes it must contain.
struct OverlayModule {
  std::string_view guestPath;
  std::string_view discPath;
  std::string_view identityName;
  std::uint32_t fileBytes;
  std::string_view retailSha256;
};

// The code-image owner of one fixed-address overlay slot. Modules are alternative contents of the
// slot: exactly one identity is active at a time, it is registered only after the transferred bytes
// equal the authenticated disc file, and it is retired (with translated-code invalidation over the
// whole window) whenever the slot is reloaded.
class OverlaySlot {
public:
  OverlaySlot(std::uint32_t loadAddress, std::uint32_t windowBytes, std::vector<OverlayModule> modules);

  std::uint32_t loadAddress() const {
    return loadAddress_;
  }
  const OverlayModule &module(std::size_t index) const {
    return modules_.at(index);
  }
  std::optional<std::size_t> matchLoad(Core &core, std::uint32_t guestPath, std::uint32_t destination) const;
  std::optional<lucent::content::Sha256>
  authenticateSource(std::size_t index, std::span<const std::uint8_t> discBytes, std::string &why) const;
  void retire(Core &core);
  bool publish(Core &core, std::size_t index, std::span<const std::uint8_t> discBytes, std::string &why);
  std::optional<psx::cpu::ImageIdentity> activeIdentity() const {
    return active_;
  }

private:
  std::uint32_t loadAddress_;
  std::uint32_t windowBytes_;
  std::vector<OverlayModule> modules_;
  std::optional<psx::cpu::ImageIdentity> active_;
};

} // namespace ts2
