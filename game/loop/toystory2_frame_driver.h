#pragma once

#include <memory>

class FrameDriver;
class Game;

namespace ts2 {

std::unique_ptr<FrameDriver> createFrameDriver(Game &game);

} // namespace ts2
