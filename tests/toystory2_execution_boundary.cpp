// Hermetic title execution boundaries: a guest call can span bounded Lightrec slices, and a
// loaded MEMORY module becomes executable only after its transferred bytes match the disc source.
// Synthetic bytes here prove state transitions; real-title reach is a separate runtime check.

#include "game.h"
#include "guest_execution.h"
#include "image_identity.h"
#include "lightrec_executor.h"
#include "overlay/shared_slot_image.h"
#include "testutil.h"
#include "toystory2_runtime.h"

#include <algorithm>
#include <lucent/content.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

static void test_finite_guest_call_continues_guest_state_and_preserves_return_sentinel() {
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

  const ts2::GuestCall call{entry, returnAddress, {}, std::nullopt, "synthetic finite call"};
  const auto result = ts2::executeFiniteGuestCall(
      core, call, psx::cpu::ExecutionBudget::fromCycles(32), ts2::kFiniteInitializationSliceLimit);
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
  const auto bounded = ts2::executeFiniteGuestCall(core, call, psx::cpu::ExecutionBudget::fromCycles(32), 1);
  CHECK_EQ(bounded.reason, psx::cpu::ExecutionExitReason::BudgetExhausted);
  CHECK(bounded.detail == "finite guest call exceeded its slice bound");
  CHECK_EQ(core.r[23], 0u);

  constexpr std::uint32_t nonReturning = entry + 0x80u;
  core.mem_w32(nonReturning, 0x26520001u);                                             // addiu s2, s2, 1
  core.mem_w32(nonReturning + 4u, 0x08000000u | ((nonReturning >> 2u) & 0x03FFFFFFu)); // j nonReturning
  core.mem_w32(nonReturning + 8u, 0u);
  core.r[18] = 0;
  const ts2::GuestCall nonReturningCall{nonReturning, returnAddress, {}, std::nullopt, "synthetic non-return"};
  const auto refused =
      ts2::executeFiniteGuestCall(core, nonReturningCall, psx::cpu::ExecutionBudget::fromCycles(32), 2);
  CHECK_EQ(refused.reason, psx::cpu::ExecutionExitReason::BudgetExhausted);
  CHECK(refused.detail == "finite guest call exceeded its slice bound");
  CHECK(core.r[18] > 0u);
  CHECK_EQ(core.r[23], 0u);
  CHECK_EQ(core.lightrecExecutor().counters().fallback.calls, 0u);
}

static void test_memory_overlay_publication_authenticates_bytes_and_retires_replaced_identity() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  using Image = ts2::SharedSlotImage;
  constexpr auto memory = Image::Kind::Memory;

  constexpr std::uint32_t guestPath = 0x80160000u;
  const auto path = Image::spec(memory).guestPath;
  for (std::size_t index = 0; index < path.size(); ++index) {
    core.mem_w8(guestPath + static_cast<std::uint32_t>(index), path[index]);
  }
  core.mem_w8(guestPath + static_cast<std::uint32_t>(path.size()), 0);
  CHECK(Image::matchLoad(core, guestPath, Image::kLoadAddress) == memory);
  CHECK(!Image::matchLoad(core, guestPath, Image::kLoadAddress + 4u));
  core.mem_w8(guestPath, 'x');
  CHECK(!Image::matchLoad(core, guestPath, Image::kLoadAddress));

  std::vector<std::uint8_t> discBytes(Image::kMemoryFileBytes);
  for (std::size_t index = 0; index < discBytes.size(); ++index) {
    discBytes[index] = static_cast<std::uint8_t>(index * 73u + 9u);
  }
  const auto fixtureSha256 = lucent::content::sha256_hex(lucent::content::sha256(std::as_bytes(std::span(discBytes))));
  Image image{fixtureSha256};
  Image retailImage;
  std::string why;
  CHECK(!retailImage.publish(core, memory, discBytes, why));
  CHECK(why.find("SHA-256") != std::string::npos);
  CHECK(!retailImage.activeIdentity().has_value());

  constexpr std::uint32_t physical = Image::kLoadAddress & 0x1FFFFFFFu;
  std::copy(discBytes.begin(), discBytes.end(), core.ram + physical);
  const auto beforeInvalidations = core.lightrecExecutor().counters().invalidations;
  CHECK(image.publish(core, memory, discBytes, why));
  const auto first = image.activeIdentity();
  CHECK(first.has_value());
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == first);
  CHECK_EQ(core.lightrecExecutor().counters().invalidations, beforeInvalidations + 1u);

  // The same authenticated image can be loaded again; its new residency gets a new generation.
  std::copy(discBytes.begin(), discBytes.end(), core.ram + physical);
  CHECK(image.publish(core, memory, discBytes, why));
  const auto second = image.activeIdentity();
  CHECK(second.has_value());
  CHECK(first != second);
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == second);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);

  auto alteredDiscBytes = discBytes;
  alteredDiscBytes[100] ^= 1u;
  const auto beforeRejectedInvalidations = core.lightrecExecutor().counters().invalidations;
  CHECK(!image.publish(core, memory, alteredDiscBytes, why));
  CHECK(why.find("SHA-256") != std::string::npos);
  CHECK(image.activeIdentity() == second);
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == second);
  CHECK_EQ(core.lightrecExecutor().counters().invalidations, beforeRejectedInvalidations);

  image.retire(core); // the other module sharing this slot must not inherit MEMORY's identity
  CHECK(!image.activeIdentity().has_value());
  CHECK(!core.imageCatalog().resolve(0x800E10E4u).has_value());
  CHECK(image.publish(core, memory, discBytes, why));

  core.mem_w8(Image::kLoadAddress + 100u, discBytes[100] ^ 1u);
  CHECK(!image.publish(core, memory, discBytes, why));
  CHECK(why.find("transferred BITS/MEMORY.BIN bytes") != std::string::npos);
  CHECK(!image.activeIdentity().has_value());
  CHECK(!core.imageCatalog().resolve(0x800E10E4u).has_value());
}

static void test_shared_slot_authenticates_fmv_and_replaces_memory_without_identity_leak() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  using Image = ts2::SharedSlotImage;
  constexpr auto memory = Image::Kind::Memory;
  constexpr auto fmv = Image::Kind::Fmv;
  constexpr std::uint32_t guestPath = 0x80160000u;
  constexpr std::uint32_t fmvEntry = 0x800D6628u;
  constexpr std::uint32_t fmvOnlyAddress = 0x80100000u;
  constexpr std::uint32_t physical = Image::kLoadAddress & 0x1FFFFFFFu;

  const auto setPath = [&](std::string_view path) {
    for (std::size_t offset = 0; offset < path.size(); ++offset) {
      core.mem_w8(guestPath + static_cast<std::uint32_t>(offset), path[offset]);
    }
    core.mem_w8(guestPath + static_cast<std::uint32_t>(path.size()), 0);
  };
  setPath(Image::spec(fmv).guestPath);
  CHECK(Image::matchLoad(core, guestPath, Image::kLoadAddress) == fmv);
  CHECK(!Image::matchLoad(core, guestPath, Image::kLoadAddress + 4u));
  setPath("fmv\\fmv.bix");
  CHECK(!Image::matchLoad(core, guestPath, Image::kLoadAddress));
  setPath(Image::spec(memory).guestPath);
  CHECK(Image::matchLoad(core, guestPath, Image::kLoadAddress) == memory);

  std::vector<std::uint8_t> memoryBytes(Image::kMemoryFileBytes);
  std::vector<std::uint8_t> fmvBytes(Image::kFmvFileBytes);
  for (std::size_t offset = 0; offset < memoryBytes.size(); ++offset) {
    memoryBytes[offset] = static_cast<std::uint8_t>(offset * 73u + 9u);
  }
  for (std::size_t offset = 0; offset < fmvBytes.size(); ++offset) {
    fmvBytes[offset] = static_cast<std::uint8_t>(offset * 29u + 17u);
  }
  const auto digestHex = [](std::span<const std::uint8_t> bytes) {
    return lucent::content::sha256_hex(lucent::content::sha256(std::as_bytes(bytes)));
  };
  Image image{digestHex(memoryBytes), digestHex(fmvBytes)};
  Image retailImage;
  std::string why;
  CHECK(!retailImage.publish(core, fmv, fmvBytes, why));
  CHECK(why.find("SHA-256") != std::string::npos);
  CHECK(!retailImage.activeIdentity());

  std::copy(memoryBytes.begin(), memoryBytes.end(), core.ram + physical);
  CHECK(image.publish(core, memory, memoryBytes, why));
  const auto memoryIdentity = image.activeIdentity();
  CHECK(memoryIdentity.has_value());
  CHECK(!core.imageCatalog().resolve(fmvOnlyAddress));

  std::copy(fmvBytes.begin(), fmvBytes.end(), core.ram + physical);
  CHECK(image.publish(core, fmv, fmvBytes, why));
  const auto fmvIdentity = image.activeIdentity();
  CHECK(fmvIdentity.has_value());
  CHECK(fmvIdentity != memoryIdentity);
  CHECK(core.imageCatalog().resolve(fmvEntry) == fmvIdentity);
  CHECK(core.imageCatalog().resolve(fmvOnlyAddress) == fmvIdentity);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);

  auto alteredSource = fmvBytes;
  alteredSource[100] ^= 1u;
  const auto invalidations = core.lightrecExecutor().counters().invalidations;
  CHECK(!image.publish(core, fmv, alteredSource, why));
  CHECK(why.find("SHA-256") != std::string::npos);
  CHECK(image.activeIdentity() == fmvIdentity);
  CHECK_EQ(core.lightrecExecutor().counters().invalidations, invalidations);
  CHECK(!image.publish(core, fmv, std::span(fmvBytes).first(fmvBytes.size() - 1u), why));
  CHECK(why.find("length") != std::string::npos);
  CHECK(image.activeIdentity() == fmvIdentity);

  core.mem_w8(Image::kLoadAddress + 100u, fmvBytes[100] ^ 1u);
  CHECK(!image.publish(core, fmv, fmvBytes, why));
  CHECK(why.find("transferred FMV") != std::string::npos);
  CHECK(!image.activeIdentity());
  CHECK(!core.imageCatalog().resolve(fmvEntry));

  std::copy(fmvBytes.begin(), fmvBytes.end(), core.ram + physical);
  CHECK(image.publish(core, fmv, fmvBytes, why));
  std::copy(memoryBytes.begin(), memoryBytes.end(), core.ram + physical);
  CHECK(image.publish(core, memory, memoryBytes, why));
  CHECK(core.imageCatalog().resolve(fmvEntry) == image.activeIdentity());
  CHECK(!core.imageCatalog().resolve(fmvOnlyAddress));
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);
}

int main() {
  RUN(finite_guest_call_continues_guest_state_and_preserves_return_sentinel);
  RUN(memory_overlay_publication_authenticates_bytes_and_retires_replaced_identity);
  RUN(shared_slot_authenticates_fmv_and_replaces_memory_without_identity_leak);
  return pt_summary();
}
