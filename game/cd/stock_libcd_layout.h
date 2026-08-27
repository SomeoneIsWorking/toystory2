#pragma once

#include <cstdint>

namespace ts2::cd {

// Identity-checked SLUS_008.93 stock-libcd entry points and state. These facts are derived by
// tools/verify_cd_command.py from the retail instruction stream; the title configuration and its
// boundary test both consume this one typed authority.
struct StockLibcdLayout {
  uint32_t command;
  uint32_t sync;
  uint32_t getSector;
  uint32_t read;
  uint32_t readSync;
  uint32_t searchFile;
  uint32_t readyCallback;
  uint32_t lastPosition;
  uint32_t libraryWindowLo;
  uint32_t libraryWindowHi;
};

inline constexpr StockLibcdLayout kStockLibcdLayout{
    .command = 0x80091DE4u,
    .sync = 0x80091898u,
    .getSector = 0x80091108u,
    .read = 0x80093AF0u,
    .readSync = 0x80093BF4u,
    .searchFile = 0x80092AE8u,
    .readyCallback = 0x800A0808u,
    .lastPosition = 0x800A0820u,
    .libraryWindowLo = 0x80091108u,
    .libraryWindowHi = 0x80093C50u,
};

} // namespace ts2::cd
