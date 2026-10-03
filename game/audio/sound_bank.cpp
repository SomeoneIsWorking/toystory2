#include "audio/sound_bank.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "execution_exit.h"
#include "native_dispatch.h"

#include <cstdint>

namespace ts2::audio {
namespace {

// The routine that contains the assertion, whose original body this owner runs under a bound.
constexpr std::uint32_t kSoundBankProcessor = 0x8007F108u;

void soundBankProcessorOverride(Core *core) {
  psx::cpu::callOriginalToReturnResuming(
      *core, kSoundBankProcessor, psx::cpu::ExecutionBudget::currentTurn(*core), "guest sound-bank processor");
}

} // namespace

void SoundBankProcessor::install(Core &core) {
  psx::cpu::installNativeOverride(core, kSoundBankProcessor, "sound-bank-processor", soundBankProcessorOverride);
}

} // namespace ts2::audio
