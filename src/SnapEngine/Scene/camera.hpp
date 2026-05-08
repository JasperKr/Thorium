#pragma once

namespace Engine::Scene {

struct Camera {
  float VerticalFOV{};
  float AspectRatio{};
  float NearPlane{};
  float FarPlane{};
};

} // namespace Engine::Scene