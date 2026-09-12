#pragma once

#include "image_identity.h"

#include <lucent/content.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

class Core;

namespace ts2 {

// MEMORY and FMV are mutually exclusive executable contents of one guest-RAM slot.
class SharedSlotImage {
public:
  enum class Kind : std::uint8_t { Memory, Fmv };

  struct Spec {
    Kind kind;
    std::string_view guestPath;
    std::string_view discPath;
    std::string_view identityName;
    std::uint32_t fileBytes;
  };

  static constexpr std::uint32_t kLoadAddress = 0x800D5D20u;
  static constexpr std::uint32_t kMemoryFileBytes = 63312u;
  static constexpr std::uint32_t kFmvFileBytes = 510960u;
  static constexpr std::string_view kMemoryGuestPath = "bits\\memory.bin";
  static constexpr std::string_view kFmvGuestPath = "fmv\\fmv.bin";
  static constexpr std::string_view kMemoryRetailSha256 =
      "ddd2e8bf26b62ae2d2414d9251ae1a38ed5d5813640f244eab08584c55267f9d";
  static constexpr std::string_view kFmvRetailSha256 =
      "acaf125051be7ea96e41593bc1c1a40b695449924e990bda855cec38f91e8ee3";

  explicit SharedSlotImage(std::string_view memorySha256 = kMemoryRetailSha256,
                           std::string_view fmvSha256 = kFmvRetailSha256)
      : expectedSha256_{std::string(memorySha256), std::string(fmvSha256)} {}

  static const Spec &spec(Kind kind);
  static std::optional<Kind> matchLoad(Core &core, std::uint32_t guestPath, std::uint32_t destination);
  std::optional<lucent::content::Sha256>
  authenticateSource(Kind kind, std::span<const std::uint8_t> discBytes, std::string &why) const;
  void retire(Core &core);
  bool publish(Core &core, Kind kind, std::span<const std::uint8_t> discBytes, std::string &why);
  std::optional<psx::cpu::ImageIdentity> activeIdentity() const;

private:
  static constexpr std::size_t index(Kind kind) {
    return static_cast<std::size_t>(kind);
  }

  std::array<std::string, 2> expectedSha256_;
  std::optional<psx::cpu::ImageIdentity> active_;
};

void installSharedSlotImageObserver(Core &core);

} // namespace ts2
