// frame_driver.cpp — the frame turn itself: what the framework calls once per frame.

#include "frame/frame_driver.h"

#include "core.h"
#include "frame/frame_boundary.h"
#include "frame/resident_frame.h"
#include "game.h"

#include <memory>

namespace ts2 {
namespace {

class ToyStory2FrameDriver final : public FrameDriver {
public:
  explicit ToyStory2FrameDriver(Core &core) : state_(core) {}

  void stepFrame(Core &core, uint32_t frame) override {
    CoreFrameBoundary boundary(core, state_);
    stepResidentFrame(boundary, frame);
  }

private:
  FrameCallState state_;
};

} // namespace

std::unique_ptr<FrameDriver> createFrameDriver(Game &game) {
  return std::make_unique<ToyStory2FrameDriver>(game.core);
}

} // namespace ts2
