#pragma once

#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
namespace SnapEngine::Renderer {

struct LightProbe {
  Math::Vec3 Position;
  float Radius;
  float InnerRadius;

  int32_t EnvironmentMapIndex;

  auto Render() -> Error;
};

} // namespace SnapEngine::Renderer