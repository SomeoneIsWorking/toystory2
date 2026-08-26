#pragma once

#include "game_iface.h"

namespace ts2 {

// Process-lifetime owner of Toy Story 2's framework-facing behavior. The legacy base is temporary:
// it keeps independently measured guest facts available while shared algorithms migrate away from
// GameConfig and the remaining compatibility callbacks acquire typed runtime interfaces.
class ToyStory2Runtime final : public LegacyGameRuntimeAdapter {
public:
  ToyStory2Runtime();

  RenderCapabilities renderCapabilities() const override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
};

} // namespace ts2
