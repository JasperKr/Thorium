#pragma once

#include "Modules/Math/vector.hpp"
#include "light.hpp"
namespace Engine::Scene {

constexpr size_t DirectionalLightShadowmapCascadeCount = 4;
constexpr size_t DirectionalLightShadowmapResolution = 2048;

struct DirectionalLight : public Light {
  Math::Vec3 Direction{};
};

} // namespace Engine::Scene