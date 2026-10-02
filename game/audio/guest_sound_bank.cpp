#include "audio/guest_sound_bank.h"

#include "core.h"
#include "core/guest_execution.h"

#include <cstdint>

namespace ts2::audio {
namespace {

// The routine that contains the assertion, whose original body this owner runs under a bound.
constexpr std::uint32_t kSoundBankProcessor = 0x8007F108u;

void soundBankProcessorOverride(Core *core) {
  callOriginalToReturnResuming(*core, kSoundBankProcessor, "guest sound-bank processor");
}

} // namespace

void installSoundBankProcessorOverride(Core &core) {
  installResidentOverride(core, kSoundBankProcessor, "sound-bank-processor", soundBankProcessorOverride);
}

} // namespace ts2::audio