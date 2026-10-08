#pragma once

#include "overlay/overlay_slot.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ts2 {

// MEMORY and FMV are mutually exclusive executable contents of the one guest-RAM slot at 0x800D5D20.
struct SharedSlotImage {
  enum Module : std::size_t { Memory, Fmv };

  static constexpr std::uint32_t kLoadAddress = 0x800D5D20u;
  static constexpr std::uint32_t kMemoryFileBytes = 63312u;
  static constexpr std::uint32_t kFmvFileBytes = 510960u;
  static constexpr std::string_view kMemoryGuestPath = "bits\\memory.bin";
  static constexpr std::string_view kFmvGuestPath = "fmv\\fmv.bin";
  static constexpr std::string_view kMemoryRetailSha256 =
      "ddd2e8bf26b62ae2d2414d9251ae1a38ed5d5813640f244eab08584c55267f9d";
  static constexpr std::string_view kFmvRetailSha256 =
      "acaf125051be7ea96e41593bc1c1a40b695449924e990bda855cec38f91e8ee3";
  static constexpr std::string_view kFmvIdentityName = "FMV/FMV.BIN";

  static std::vector<OverlayModule> retailModules() {
    return {
        {kMemoryGuestPath, "\\BITS\\MEMORY.BIN;1", "BITS/MEMORY.BIN", kMemoryFileBytes, kMemoryRetailSha256},
        {kFmvGuestPath, "\\FMV\\FMV.BIN;1", kFmvIdentityName, kFmvFileBytes, kFmvRetailSha256},
    };
  }

  static OverlaySlot makeSlot(std::vector<OverlayModule> modules = retailModules()) {
    return OverlaySlot(kLoadAddress, std::max(kMemoryFileBytes, kFmvFileBytes), std::move(modules));
  }
};

} // namespace ts2
