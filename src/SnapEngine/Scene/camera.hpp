#pragma once

namespace Engine {

struct Camera {
  float VerticalFOV{};
  float AspectRatio{};
  float NearPlane{};
  float FarPlane{};
};

} // namespace Engine