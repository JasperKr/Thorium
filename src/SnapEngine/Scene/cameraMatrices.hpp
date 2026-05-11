#pragma once

#include "Modules/Math/matrix.hpp"
namespace Engine {

struct CameraMatrices {
  Math::Matrix4x4 RotationMatrix;
  Math::Matrix4x4 InverseRotationMatrix;
  Math::Matrix4x4 ViewMatrix;
  Math::Matrix4x4 InverseViewMatrix;
  Math::Matrix4x4 ProjectionMatrix;
  Math::Matrix4x4 InverseProjectionMatrix;
  Math::Matrix4x4 ViewProjectionMatrix;
  Math::Matrix4x4 InverseViewProjectionMatrix;
  Math::Matrix4x4 RotationProjectionMatrix;
  Math::Matrix4x4 InverseRotationProjectionMatrix;

  [[nodiscard]] auto GetFrustum() const -> struct Frustum;
  auto Update() -> void;
};

} // namespace Engine