#pragma once

class Core;

namespace ts2 {

// Also installs guest VBlank (0x80039D60), which nothing else dispatches.
class GraphicsSync {
public:
  void install(Core &core);
};

} // namespace ts2
