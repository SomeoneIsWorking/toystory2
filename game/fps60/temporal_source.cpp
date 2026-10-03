#include "fps60/temporal_source.h"

#include "runtime/toystory2_context.h"

namespace ts2::render {

bool ResidentTemporalSource::continuousWithPrevious(Core &core) {
  return context(core).camera.continuous();
}

} // namespace ts2::render
