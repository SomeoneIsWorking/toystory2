#pragma once

class Core;

namespace ts2::audio {

// Its size assertion is an unconditional `VSync` loop, so the bound turns a bad bank into a refusal.
class SoundBankProcessor {
public:
  void install(Core &core);
};

} // namespace ts2::audio
