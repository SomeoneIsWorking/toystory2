#pragma once

#include "overlay/overlay_slot.h"

#include <array>
#include <cstdint>

namespace ts2 {

// LEVEL{,1}.BIN of the ten levels are alternative contents of the guest-RAM slot at 0x800D12C0
// (19,040-byte window). The four-byte LEVEL00 placeholder holds no code and is not a module.
struct LevelSlotImage {
  static constexpr std::uint32_t kLoadAddress = 0x800D12C0u;
  static constexpr std::uint32_t kWindowBytes = 19040u;
  static constexpr std::size_t kModuleCount = 20;

  static constexpr std::array<OverlayModule, kModuleCount> kRetailModules{{
      {"level01\\level.bin",
       "\\LEVEL01\\LEVEL.BIN;1",
       "LEVEL01/LEVEL.BIN",
       13868u,
       "f39c54c0c7f92efc6a903e808a499ccc66f5ba7da26d795ab4e0fae53fb63ade"},
      {"level01\\level1.bin",
       "\\LEVEL01\\LEVEL1.BIN;1",
       "LEVEL01/LEVEL1.BIN",
       18744u,
       "a67bcc429b13b26d2f3209eff5e860aaafb5baa595075581e10d2ee9880cd490"},
      {"level02\\level.bin",
       "\\LEVEL02\\LEVEL.BIN;1",
       "LEVEL02/LEVEL.BIN",
       19040u,
       "6b1a0149d609375e84977267bc45607d9ba5d152426c2729109a4b6150a06c1e"},
      {"level02\\level1.bin",
       "\\LEVEL02\\LEVEL1.BIN;1",
       "LEVEL02/LEVEL1.BIN",
       5248u,
       "60c0ce96f725027057ad2a3cdcc41b5319b7afbad7d79219464d4c59014da3be"},
      {"level03\\level.bin",
       "\\LEVEL03\\LEVEL.BIN;1",
       "LEVEL03/LEVEL.BIN",
       5276u,
       "acf01f10c1a8207e073990df7520170b055a0131264e415adfcd176083a1b995"},
      {"level03\\level1.bin",
       "\\LEVEL03\\LEVEL1.BIN;1",
       "LEVEL03/LEVEL1.BIN",
       10376u,
       "0e640e2ada303ed07e3027d0d591c15951d82d42137b93f452265f6d17f82d90"},
      {"level04\\level.bin",
       "\\LEVEL04\\LEVEL.BIN;1",
       "LEVEL04/LEVEL.BIN",
       16724u,
       "818fd740248fa3a8d6e268736bea54272919a040eeda60ee128c3bbb77dde58b"},
      {"level04\\level1.bin",
       "\\LEVEL04\\LEVEL1.BIN;1",
       "LEVEL04/LEVEL1.BIN",
       12232u,
       "b57756b4b087c36f24239a982ee6568470975d0f5fc42b04b622c7f7e18dd2c1"},
      {"level05\\level.bin",
       "\\LEVEL05\\LEVEL.BIN;1",
       "LEVEL05/LEVEL.BIN",
       13432u,
       "cae2800cfb864afb8c7f9fe80abe49b4d4d759d224d4e75f6100ba1d5d4e561e"},
      {"level05\\level1.bin",
       "\\LEVEL05\\LEVEL1.BIN;1",
       "LEVEL05/LEVEL1.BIN",
       8148u,
       "8ba76252dd43433c8eadd2322ba506cea02b24cf4f1e4be3792a1d946b0264de"},
      {"level06\\level.bin",
       "\\LEVEL06\\LEVEL.BIN;1",
       "LEVEL06/LEVEL.BIN",
       3608u,
       "a3bfc8e906de78093a2aee83a88a7cbf3d0764cf36e8b2bba3ac066b4e050c57"},
      {"level06\\level1.bin",
       "\\LEVEL06\\LEVEL1.BIN;1",
       "LEVEL06/LEVEL1.BIN",
       624u,
       "66063b31f7487dd14c498d5ce95dc42345741ec26ecc0716f09179c132155c69"},
      {"level07\\level.bin",
       "\\LEVEL07\\LEVEL.BIN;1",
       "LEVEL07/LEVEL.BIN",
       12884u,
       "abac1e83ad294406b7283fe9051068b88f8d3eebdc5b7280441eb51ccda8bba3"},
      {"level07\\level1.bin",
       "\\LEVEL07\\LEVEL1.BIN;1",
       "LEVEL07/LEVEL1.BIN",
       24u,
       "d0143f7107601a609bd70215084eaec9ffa3fab24cd1f08740918e05c0d90d3c"},
      {"level08\\level.bin",
       "\\LEVEL08\\LEVEL.BIN;1",
       "LEVEL08/LEVEL.BIN",
       12908u,
       "666ad4abd6399618ff5729802a73e8e113620605a9a775c41e942ed67f8f331c"},
      {"level08\\level1.bin",
       "\\LEVEL08\\LEVEL1.BIN;1",
       "LEVEL08/LEVEL1.BIN",
       24u,
       "10d178b18708c7eaf411b0d9ef17b4f2357a403c627540ca414d1b56b39e828b"},
      {"level09\\level.bin",
       "\\LEVEL09\\LEVEL.BIN;1",
       "LEVEL09/LEVEL.BIN",
       7608u,
       "f9ff86cdb25a079a5db0122fba26651d390a28d3b09744a090298bd0d63bd510"},
      {"level09\\level1.bin",
       "\\LEVEL09\\LEVEL1.BIN;1",
       "LEVEL09/LEVEL1.BIN",
       24u,
       "951a7e58e1f1daa3e281c1ff1cfa405d9f12154695079f394119522c4362e830"},
      {"level10\\level.bin",
       "\\LEVEL10\\LEVEL.BIN;1",
       "LEVEL10/LEVEL.BIN",
       18392u,
       "ac7979ee333cc84d6ab6f665e1d057b3d0faa976eb3c7a1cc526e99c0334a956"},
      {"level10\\level1.bin",
       "\\LEVEL10\\LEVEL1.BIN;1",
       "LEVEL10/LEVEL1.BIN",
       2652u,
       "1e980b22a8455875d98ea4e9f074c2048c6a794a03b3915f8a5a050f457c5f1e"},
  }};

  static OverlaySlot makeSlot() {
    return OverlaySlot(kLoadAddress, kWindowBytes, {kRetailModules.begin(), kRetailModules.end()});
  }
};

} // namespace ts2
