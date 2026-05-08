#pragma once

#include "Modules/Math/vector.hpp"
namespace Engine::Scene {

struct RectangleLight {
  Math::Vec2 Size{1.0F, 1.0F};
  float Range{};
};

} // namespace Engine::Scene