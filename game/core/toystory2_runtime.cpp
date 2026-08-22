#include "toystory2_runtime.h"

#include "cfg.h"
#include "core.h"
#include "legacy_game_interface.h"
#include "sync/field_clock.h"

#include <cstdlib>

namespace ts2 {

ToyStory2Runtime::ToyStory2Runtime() : LegacyGameRuntimeAdapter(legacy::measuredConfig, legacy::compatibilityHooks) {}

void ToyStory2Runtime::registerOverrides(Game &) {
  // RE-10 owns the host field boundary only. The retail graphics initializer and VBlank callback
  // remain generated guest code; field_clock supplies the asynchronous host turn between them.
  ts2_field_clock_install();
}

void ToyStory2Runtime::bootInit(Core &core) {
  const GameConfig *config = legacyConfigForMigration();
  if (!config || !config->gameMain) {
    cfg_loge("boot",
             "the measured RE-01 gameMain entry is absent from Toy Story 2's legacy program facts; "
             "refusing to dispatch address 0");
    std::abort();
  }
  cfg_logi("boot", "dispatching guest main() 0x%08X on the recompiled substrate", config->gameMain);
  rec_dispatch(&core, config->gameMain);
}

} // namespace ts2
