#pragma once

#include "loop/outer_loop.h"

#include <cstdint>

class Core;

namespace ts2 {

// Finite owner for retail 0x8007BEC4 and its nested 0x8007C344 field loop. Each call performs at
// most one authored transition-field iteration; no guest wait controls the host frame lifetime.
class ResidentPreparation {
public:
  ResidentPreparationProgress step(Core &core, uint32_t level, int playbackMode);

private:
  enum class Phase {
    begin,
    transition,
  };

  void begin(Core &core, uint32_t level, int playbackMode);
  ResidentPreparationProgress stepTransition(Core &core);
  bool finish(Core &core);

  Phase phase_ = Phase::begin;
  uint32_t level_ = 0;
  int playbackMode_ = 0;
  int bootFieldsRemaining_ = -1;
  int fadeFieldsRemaining_ = 0;
  int fadePosition_ = 0;
  uint32_t cycle_ = 0;
  bool fadeActive_ = false;
  bool interrupted_ = false;
};

} // namespace ts2
