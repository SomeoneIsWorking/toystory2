// Hermetic title execution boundaries: a guest call can span bounded Lightrec slices, and a
// loaded MEMORY module becomes executable only after its transferred bytes match the disc source.
// Synthetic bytes here prove state transitions; real-title reach is a separate runtime check.

#include "execution/guest_execution.h"
#include "game.h"
#include "image_identity.h"
#include "lightrec_executor.h"
#include "overlay/overlay_images.h"
#include "runtime/toystory2_runtime.h"
#include "testutil.h"

#include <algorithm>
#include <lucent/content.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

static void writeGuestPath(Core &core, std::uint32_t address, std::string_view path) {
  for (std::size_t offset = 0; offset < path.size(); ++offset) {
    core.mem_w8(address + static_cast<std::uint32_t>(offset), path[offset]);
  }
  core.mem_w8(address + static_cast<std::uint32_t>(path.size()), 0);
}

static std::string digestHex(std::span<const std::uint8_t> bytes) {
  return lucent::content::sha256_hex(lucent::content::sha256(std::as_bytes(bytes)));
}

static std::vector<std::uint8_t> fixtureBytes(std::size_t size, unsigned multiplier, unsigned offset) {
  std::vector<std::uint8_t> bytes(size);
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::uint8_t>(index * multiplier + offset);
  }
  return bytes;
}

// A slot whose modules carry the digests of the synthetic fixtures instead of the retail ones.
static ts2::OverlaySlot fixtureSlot(std::uint32_t loadAddress,
                                    std::uint32_t window,
                                    std::vector<ts2::OverlayModule> modules,
                                    const std::vector<std::vector<std::uint8_t>> &contents,
                                    std::vector<std::string> &digests) {
  digests.clear();
  for (const auto &bytes : contents) {
    digests.push_back(digestHex(bytes));
  }
  for (std::size_t index = 0; index < modules.size(); ++index) {
    modules[index].retailSha256 = digests[index];
  }
  return ts2::OverlaySlot(loadAddress, window, std::move(modules));
}

static void test_memory_overlay_publication_authenticates_bytes_and_retires_replaced_identity() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  using Image = ts2::SharedSlotImage;
  constexpr auto memory = Image::Memory;

  constexpr std::uint32_t guestPath = 0x80160000u;
  const auto retail = Image::makeSlot();
  writeGuestPath(core, guestPath, Image::kMemoryGuestPath);
  CHECK(retail.matchLoad(core, guestPath, Image::kLoadAddress) == memory);
  CHECK(!retail.matchLoad(core, guestPath, Image::kLoadAddress + 4u));
  core.mem_w8(guestPath, 'x');
  CHECK(!retail.matchLoad(core, guestPath, Image::kLoadAddress));

  const auto discBytes = fixtureBytes(Image::kMemoryFileBytes, 73u, 9u);
  std::vector<std::string> digests;
  auto image = fixtureSlot(Image::kLoadAddress,
                           Image::kFmvFileBytes,
                           Image::retailModules(),
                           {discBytes, fixtureBytes(Image::kFmvFileBytes, 29u, 17u)},
                           digests);
  auto retailImage = Image::makeSlot();
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
  constexpr auto memory = Image::Memory;
  constexpr auto fmv = Image::Fmv;
  constexpr std::uint32_t guestPath = 0x80160000u;
  constexpr std::uint32_t fmvEntry = 0x800D6628u;
  constexpr std::uint32_t fmvOnlyAddress = 0x80100000u;
  constexpr std::uint32_t physical = Image::kLoadAddress & 0x1FFFFFFFu;

  const auto retail = Image::makeSlot();
  writeGuestPath(core, guestPath, Image::kFmvGuestPath);
  CHECK(retail.matchLoad(core, guestPath, Image::kLoadAddress) == fmv);
  CHECK(!retail.matchLoad(core, guestPath, Image::kLoadAddress + 4u));
  writeGuestPath(core, guestPath, "fmv\\fmv.bix");
  CHECK(!retail.matchLoad(core, guestPath, Image::kLoadAddress));
  writeGuestPath(core, guestPath, Image::kMemoryGuestPath);
  CHECK(retail.matchLoad(core, guestPath, Image::kLoadAddress) == memory);

  const auto memoryBytes = fixtureBytes(Image::kMemoryFileBytes, 73u, 9u);
  const auto fmvBytes = fixtureBytes(Image::kFmvFileBytes, 29u, 17u);
  std::vector<std::string> digests;
  auto image =
      fixtureSlot(Image::kLoadAddress, Image::kFmvFileBytes, Image::retailModules(), {memoryBytes, fmvBytes}, digests);
  auto retailImage = Image::makeSlot();
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

static void test_level_slot_is_coresident_with_the_shared_slot_and_each_load_replaces_only_its_own_identity() {
  static ts2::ToyStory2Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  using Level = ts2::LevelSlotImage;
  using Shared = ts2::SharedSlotImage;
  constexpr std::uint32_t guestPath = 0x80160000u;
  constexpr std::size_t level01 = 0;          // level01\level.bin, 13,868 bytes
  constexpr std::size_t level01Alternate = 1; // level01\level1.bin, 18,744 bytes
  constexpr std::uint32_t levelPhysical = Level::kLoadAddress & 0x1FFFFFFFu;
  constexpr std::uint32_t sharedPhysical = Shared::kLoadAddress & 0x1FFFFFFFu;

  const auto retail = Level::makeSlot();
  writeGuestPath(core, guestPath, "LEVEL01\\Level.Bin"); // the loader's path match is case-insensitive
  CHECK(retail.matchLoad(core, guestPath, Level::kLoadAddress) == level01);
  CHECK(!retail.matchLoad(core, guestPath, Shared::kLoadAddress));
  writeGuestPath(core, guestPath, "level00\\level.bin"); // the four-byte placeholder is not a module
  CHECK(!retail.matchLoad(core, guestPath, Level::kLoadAddress));
  writeGuestPath(core, guestPath, "level01\\level1.bin");
  CHECK(retail.matchLoad(core, guestPath, Level::kLoadAddress) == level01Alternate);

  const auto firstBytes = fixtureBytes(Level::kRetailModules[level01].fileBytes, 31u, 5u);
  const auto alternateBytes = fixtureBytes(Level::kRetailModules[level01Alternate].fileBytes, 37u, 11u);
  const auto memoryBytes = fixtureBytes(Shared::kMemoryFileBytes, 73u, 9u);
  std::vector<std::string> levelDigests;
  std::vector<std::string> sharedDigests;
  std::vector<ts2::OverlayModule> levelModules{Level::kRetailModules[level01], Level::kRetailModules[level01Alternate]};
  auto level =
      fixtureSlot(Level::kLoadAddress, Level::kWindowBytes, levelModules, {firstBytes, alternateBytes}, levelDigests);
  auto shared = fixtureSlot(Shared::kLoadAddress,
                            Shared::kFmvFileBytes,
                            {Shared::retailModules()[Shared::Memory]},
                            {memoryBytes},
                            sharedDigests);

  std::string why;
  auto retailLevel = Level::makeSlot();
  std::copy(firstBytes.begin(), firstBytes.end(), core.ram + levelPhysical);
  CHECK(!retailLevel.publish(core, level01, firstBytes, why)); // synthetic bytes are not retail LEVEL01
  CHECK(why.find("SHA-256") != std::string::npos);
  CHECK(!retailLevel.activeIdentity());

  std::copy(memoryBytes.begin(), memoryBytes.end(), core.ram + sharedPhysical);
  CHECK(shared.publish(core, 0, memoryBytes, why));
  const auto memoryIdentity = shared.activeIdentity();
  CHECK(level.publish(core, 0, firstBytes, why));
  const auto firstIdentity = level.activeIdentity();
  CHECK(firstIdentity.has_value());
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
  CHECK(core.imageCatalog().resolve(0x800D12C4u) == firstIdentity);
  CHECK(core.imageCatalog().resolve(0x800D1DBCu) == firstIdentity); // the first address the retail run executed
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == memoryIdentity);
  CHECK(!core.imageCatalog().resolve(Level::kLoadAddress + Level::kRetailModules[level01].fileBytes));

  // Loading the larger alternative replaces only the LEVEL identity and a corrupted transfer retires it.
  std::copy(alternateBytes.begin(), alternateBytes.end(), core.ram + levelPhysical);
  CHECK(level.publish(core, 1, alternateBytes, why));
  CHECK(level.activeIdentity() != firstIdentity);
  CHECK(core.imageCatalog().resolve(0x800D5000u) == level.activeIdentity());
  CHECK(shared.activeIdentity() == memoryIdentity);
  core.mem_w8(Level::kLoadAddress + 7u, alternateBytes[7] ^ 1u);
  CHECK(!level.publish(core, 1, alternateBytes, why));
  CHECK(why.find("transferred LEVEL01/LEVEL1.BIN bytes") != std::string::npos);
  CHECK(!level.activeIdentity());
  CHECK(!core.imageCatalog().resolve(0x800D1DBCu));
  CHECK(core.imageCatalog().resolve(0x800E10E4u) == memoryIdentity);

  // A module larger than the window is refused rather than published over its neighbour.
  auto narrow = fixtureSlot(Level::kLoadAddress, 64u, {levelModules[0]}, {firstBytes}, levelDigests);
  std::copy(firstBytes.begin(), firstBytes.end(), core.ram + levelPhysical);
  CHECK(!narrow.publish(core, 0, firstBytes, why));
  CHECK(why.find("does not fit") != std::string::npos);
}

int main() {
  RUN(finite_guest_call_continues_guest_state_and_preserves_return_sentinel);
  RUN(memory_overlay_publication_authenticates_bytes_and_retires_replaced_identity);
  RUN(shared_slot_authenticates_fmv_and_replaces_memory_without_identity_leak);
  RUN(level_slot_is_coresident_with_the_shared_slot_and_each_load_replaces_only_its_own_identity);
  return pt_summary();
}
