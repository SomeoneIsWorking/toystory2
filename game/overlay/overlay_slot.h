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

struct OverlayModule {
  std::string_view guestPath;
  std::string_view discPath;
  std::string_view identityName;
  std::uint32_t fileBytes;
  std::string_view retailSha256;
};

// Code-image owner of one fixed-address slot: one active identity, registered only after the loaded bytes
// equal the disc file, retired (with translated-code invalidation) when the slot is reloaded.
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
