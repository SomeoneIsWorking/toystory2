// GTE register numbers and the exact command words the resident drawers issue.
#pragma once

#include <cstdint>

namespace ts2::gte {

// Data registers.
inline constexpr std::uint32_t kVxy0 = 0;
inline constexpr std::uint32_t kVz0 = 1;
inline constexpr std::uint32_t kVxy1 = 2;
inline constexpr std::uint32_t kVz1 = 3;
inline constexpr std::uint32_t kVxy2 = 4;
inline constexpr std::uint32_t kVz2 = 5;
inline constexpr std::uint32_t kRgbc = 6;
inline constexpr std::uint32_t kOtz = 7;
inline constexpr std::uint32_t kIr0 = 8;
inline constexpr std::uint32_t kIr1 = 9;
inline constexpr std::uint32_t kIr2 = 10;
inline constexpr std::uint32_t kIr3 = 11;
inline constexpr std::uint32_t kSxy0 = 12;
inline constexpr std::uint32_t kSxy1 = 13;
inline constexpr std::uint32_t kSxy2 = 14;
inline constexpr std::uint32_t kSz1 = 17;
inline constexpr std::uint32_t kRgb2 = 22;
inline constexpr std::uint32_t kMac0 = 24;
inline constexpr std::uint32_t kMac1 = 25;
inline constexpr std::uint32_t kMac2 = 26;
inline constexpr std::uint32_t kMac3 = 27;

// Commands, as encoded in SLUS_008.93.
inline constexpr std::uint32_t kRtps = 0x4A180001u;
inline constexpr std::uint32_t kRtpt = 0x4A280030u;
inline constexpr std::uint32_t kNclip = 0x4B400006u;
inline constexpr std::uint32_t kAvsz3 = 0x4B58002Du;
inline constexpr std::uint32_t kAvsz4 = 0x4B68002Eu;
inline constexpr std::uint32_t kDcpl = 0x4A680029u;
inline constexpr std::uint32_t kCc = 0x4B38041Cu; // lm=1

} // namespace ts2::gte
