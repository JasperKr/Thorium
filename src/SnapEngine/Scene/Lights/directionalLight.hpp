#pragma once

#include "Modules/Math/vector.hpp"
namespace Engine::Scene {

constexpr size_t DirectionalLightShadowmapCascadeCount = 4;
constexpr size_t DirectionalLightShadowmapResolution = 2048;

struct PointLight {
  Math::Vec3 Direction{};
};

} // namespace Engine::Scene