#include "toystory2_context.h"

#include "core.h"

#include <cstdlib>

namespace ts2 {

ToyStory2Context &context(Core &core) {
  if (core.gameCtx == nullptr) {
    std::abort();
  }
  return *static_cast<ToyStory2Context *>(core.gameCtx);
}

} // namespace ts2
