#pragma once

#include "image_identity.h"

#include <lucent/content.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

class Core;

namespace ts2 {

class MemoryOverlayImage {
public:
  static constexpr std::uint32_t kLoadAddress = 0x800D5D20u;
  static constexpr std::uint32_t kFileBytes = 63312u;
  static constexpr std::string_view kGuestPath = "bits\\memory.bin";
  static constexpr std::string_view kDiscPath = "\\BITS\\MEMORY.BIN;1";
  static constexpr std::string_view kRetailSha256 = "ddd2e8bf26b62ae2d2414d9251ae1a38ed5d5813640f244eab08584c55267f9d";

  explicit MemoryOverlayImage(std::string_view expectedSha256 = kRetailSha256) : expectedSha256_(expectedSha256) {}

  static bool matchesLoad(Core &core, std::uint32_t guestPath, std::uint32_t destination);
  std::optional<lucent::content::Sha256> authenticateSource(std::span<const std::uint8_t> discBytes,
                                                            std::string &why) const;
  void retire(Core &core);
  bool publish(Core &core, std::span<const std::uint8_t> discBytes, std::string &why);
  std::optional<psx::cpu::ImageIdentity> activeIdentity() const;

private:
  std::string expectedSha256_;
  std::optional<psx::cpu::ImageIdentity> active_;
};

void installMemoryOverlayObserver(Core &core);

} // namespace ts2
