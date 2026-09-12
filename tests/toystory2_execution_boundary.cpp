// Hermetic title execution boundaries: a guest call can span bounded Lightrec slices, and a
// loaded MEMORY module becomes executable only after its transferred bytes match the disc source.
// Synthetic bytes here prove state transitions; real-title reach is a separate runtime check.

#include "game.h"
#include "guest_execution.h"
#include "image_identity.h"
#include "lightrec_executor.h"
#include "overlay/memory_image.h"
#include "testutil.h"
#include "toystory2_runtime.h"

#include <algorithm>
#include <lucent/content.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

static void test_finite_boot_call_continues_guest_state_and_preserves_return_sentinel() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  constexpr std::uint32_t entry = 0x80150000u;
  constexpr std::uint32_t inner = entry + 0x40u;
  constexpr std::uint32_t returnAddress = 0x80150100u;
  core.imageCatalog().activate(
      "finite-boot-test", {entry & 0x1FFFFFFFu, (returnAddress + 8u) & 0x1FFFFFFFu}, 0x46494e495445ull);
  core.mem_w32(entry, 0x03E08021u);                                      // addu s0, ra, zero
  core.mem_w32(entry + 4u, 0x0C000000u | ((inner >> 2u) & 0x03FFFFFFu)); // jal inner
  core.mem_w32(entry + 8u, 0u);
  core.mem_w32(entry + 12u, 0x02000008u); // jr s0
  core.mem_w32(entry + 16u, 0u);
  core.mem_w32(inner, 0x24020000u);       // addiu v0, zero, 0
  core.mem_w32(inner + 4u, 0x240800C8u);  // addiu t0, zero, 200
  core.mem_w32(inner + 8u, 0x24420001u);  // addiu v0, v0, 1
  core.mem_w32(inner + 12u, 0x1448FFFEu); // bne v0, t0, inner+8
  core.mem_w32(inner + 16u, 0u);
  core.mem_w32(inner + 20u, 0x03E00008u); // jr ra
  core.mem_w32(inner + 24u, 0u);
  core.mem_w32(returnAddress, 0x24177BADu); // must not execute

  const ts2::GuestCall call{entry, returnAddress, {}, std::nullopt, "synthetic finite boot"};
  const auto result = ts2::executeFiniteBootCall(core, call, psx::cpu::ExecutionBudget::fromCycles(32), 64);
  CHECK(result.returned());
  CHECK_EQ(result.guestPc, returnAddress);
  CHECK_EQ(core.r[2], 200u);
  CHECK_EQ(core.r[31], entry + 12u);
  CHECK_EQ(core.r[23], 0u);
  CHECK(result.cycles > 32u);
  CHECK(core.lightrecExecutor().counters().translatedBlocks > 0u);
  CHECK_EQ(core.lightrecExecutor().counters().fallback.calls, 0u);

  core.r[2] = 0;
  core.r[23] = 0;
  const auto bounded = ts2::executeFiniteBootCall(core, call, psx::cpu::ExecutionBudget::fromCycles(32), 1);
  CHECK_EQ(bounded.reason, psx::cpu::ExecutionExitReason::BudgetExhausted);
  CHECK(bounded.detail == "finite boot call exceeded its slice bound");
  CHECK_EQ(core.r[23], 0u);
}

static void test_memory_overlay_publication_authenticates_bytes_and_retires_replaced_identity() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  constexpr std::uint32_t guestPath = 0x80160000u;
  for (std::size_t index = 0; index < ts2::MemoryOverlayImage::kGuestPath.size(); ++index) {
    core.mem_w8(guestPath + static_cast<std::uint32_t>(index), ts2::MemoryOverlayImage::kGuestPath[index]);
  }
  core.mem_w8(guestPath + static_cast<std::uint32_t>(ts2::MemoryOverlayImage::kGuestPath.size()), 0);
  CHECK(ts2::MemoryOverlayImage::matchesLoad(core, guestPath, ts2::MemoryOverlayImage::kLoadAddress));
  CHECK(!ts2::MemoryOverlayImage::matchesLoad(core, guestPath, ts2::MemoryOverlayImage::kLoadAddress + 4u));
  core.mem_w8(guestPath, 'x');
  CHECK(!ts2::MemoryOverlayImage::matchesLoad(core, guestPath, ts2::MemoryOverlayImage::kLoadAddress));

  std::vector<std::uint8_t> discBytes(ts2::MemoryOverlayImage::kFileBytes);
  for (std::size_t index = 0; index < discBytes.size(); ++index) {
    discBytes[index] = static_cast<std::uint8_t>(index * 73u + 9u);
  }
  const auto fixtureSha256 = lucent::content::sha256_hex(lucent::content::sha256(std::as_bytes(std::span(discBytes))));
  ts2::MemoryOverlayImage image{fixtureSha256};
  ts2::MemoryOverlayImage retailImage;
  std::string why;
  CHECK(!retailImage.publish(core, discBytes, why));
  CHECK(why.find("SHA-256") != std::string::npos);
  CHECK(!retailImage.activeIdentity().has_value());

  constexpr std::uint32_t physical = ts2::MemoryOverlayImage::kLoadAddress & 0x1FFFFFFFu;
  std::copy(discBytes.begin(), discBytes.end(), core.ram + physical);
  const auto beforeInvalidations = core.lightrecExecutor().counters().invalidations;
  CHECK(image.publish(core, discBytes, why));
  const auto first = image.activeIdentity();
  CHECK(first.has_value());
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == first);
  CHECK_EQ(core.lightrecExecutor().counters().invalidations, beforeInvalidations + 1u);

  // The same retail image can be loaded again; its new residency gets a new generation.
  std::copy(discBytes.begin(), discBytes.end(), core.ram + physical);
  CHECK(image.publish(core, discBytes, why));
  const auto second = image.activeIdentity();
  CHECK(second.has_value());
  CHECK(first != second);
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == second);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);

  auto alteredDiscBytes = discBytes;
  alteredDiscBytes[100] ^= 1u;
  const auto beforeRejectedInvalidations = core.lightrecExecutor().counters().invalidations;
  CHECK(!image.publish(core, alteredDiscBytes, why));
  CHECK(why.find("SHA-256") != std::string::npos);
  CHECK(image.activeIdentity() == second);
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == second);
  CHECK_EQ(core.lightrecExecutor().counters().invalidations, beforeRejectedInvalidations);

  image.retire(core); // the other module sharing this slot must not inherit MEMORY's identity
  CHECK(!image.activeIdentity().has_value());
  CHECK(!core.imageCatalog().resolve(0x800E10E4u).has_value());
  CHECK(image.publish(core, discBytes, why));

  core.mem_w8(ts2::MemoryOverlayImage::kLoadAddress + 100u, discBytes[100] ^ 1u);
  CHECK(!image.publish(core, discBytes, why));
  CHECK(why.find("transferred MEMORY bytes") != std::string::npos);
  CHECK(!image.activeIdentity().has_value());
  CHECK(!core.imageCatalog().resolve(0x800E10E4u).has_value());
}

int main() {
  RUN(finite_boot_call_continues_guest_state_and_preserves_return_sentinel);
  RUN(memory_overlay_publication_authenticates_bytes_and_retires_replaced_identity);
  return pt_summary();
}
