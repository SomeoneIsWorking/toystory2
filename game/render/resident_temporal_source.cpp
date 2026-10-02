#include "render/resident_temporal_source.h"

#include "core/toystory2_context.h"

namespace ts2::render {

bool ResidentTemporalSource::continuousWithPrevious(Core &core) {
  return context(core).camera.ready();
}

} // namespace ts2::render
