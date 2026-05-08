#pragma once

namespace Engine::Scene {

struct SpotLight {
  float Range{};
  float InnerConeAngle{};
  float OuterConeAngle{};
};

} // namespace Engine::Scene