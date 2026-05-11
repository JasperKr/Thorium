#include "cameraMatrices.hpp"
#include "frustum.hpp"

namespace Engine {

auto CameraMatrices::GetFrustum() const -> Frustum {
  return Frustum::FromMatrices(ViewProjectionMatrix,
                               InverseViewProjectionMatrix);
}

auto CameraMatrices::Update() -> void {
  ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
  InverseViewProjectionMatrix = ViewProjectionMatrix.Inverse();
  RotationProjectionMatrix = ProjectionMatrix * RotationMatrix;
  InverseRotationProjectionMatrix = RotationProjectionMatrix.Inverse();
}

} // namespace Engine