#pragma once

class Core;

namespace ts2::audio {

// The guest's own VAB sound-bank processor, run under a bound. Every asset decode calls it to open
// and transfer one bank; its size assertion is an unconditional `VSync` loop, so this owner runs the
// guest's real body through the framework's bounded resume loop and ends the assertion as a named
// refusal instead of an unbounded spin the player would watch forever.
class SoundBankProcessor {
public:
  // Install the processor override for the resident image.
  void install(Core &core);
};

} // namespace ts2::audio
