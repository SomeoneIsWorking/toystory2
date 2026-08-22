#pragma once

struct GameConfig;
struct GameHooks;

namespace ts2::legacy {

// Compatibility facts and callbacks still consumed by generic framework algorithms through
// Core::cfg/Core::hooks. ToyStory2Runtime is the title's public behavior seam; new title behavior
// must be expressed as a runtime override or a cohesive per-Game product, not added to these bags.
extern const GameConfig &measuredConfig;
extern const GameHooks &compatibilityHooks;

} // namespace ts2::legacy
