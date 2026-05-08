#include "frustum.hpp"
#include "Modules/Math/vector.hpp"

namespace Engine::Scene {

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Frustum::IntersectsAABB(const Math::Vec3 &min, const Math::Vec3 &max,
                             bool precise) const -> bool {
  for (int i = 0; i < PlaneCount; i++) {
    int out = 0;
    auto plane = GetPlane(i);

    out += plane.Dot(Math::Vec3(min.x, min.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, min.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, max.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, max.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, min.y, max.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, min.y, max.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, max.y, max.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, max.y, max.z)) < 0.0F ? 1 : 0;

    if (out == 8) { // NOLINT
      return false;
    }
  }

  [[likely]]
  if (precise) {
    int out = 0;
    // clang-format off
    // NOLINTBEGIN
    out = 0; for (const auto &corner : Corners) { out += corner.x > max.x ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.x < min.x ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.y > max.y ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.y < min.y ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.z > max.z ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.z < min.z ? 1 : 0; }; if (out == 8) { return false; }
    // NOLINTEND
    // clang-format on
  }

  return true;
}

auto Frustum::FromMatrices(const Math::Matrix4x4 &viewProjectionMatrix,
                           const Math::Matrix4x4 &inverseViewProjectionMatrix)
    -> Frustum {
  Frustum frustum;

  // Define the corners of the NDC cube
  std::array<Math::Vec4, CornerCount> ndc_corners = {
      Math::Vec4(-1.0F, -1.0F, -1.0F, 1.0F), // NTL
      Math::Vec4(1.0F, -1.0F, -1.0F, 1.0F),  // NTR
      Math::Vec4(1.0F, -1.0F, 1.0F, 1.0F),   // NBR
      Math::Vec4(-1.0F, -1.0F, 1.0F, 1.0F),  // NBL
      Math::Vec4(-1.0F, 1.0F, -1.0F, 1.0F),  // FTL
      Math::Vec4(1.0F, 1.0F, -1.0F, 1.0F),   // FTR
      Math::Vec4(1.0F, 1.0F, 1.0F, 1.0F),    // FBR
      Math::Vec4(-1.0F, 1.0F, 1.0F, 1.0F)    // FBL
  };

  for (size_t i = 0; i < CornerCount; ++i) {
    Math::Vec4 world_pos = inverseViewProjectionMatrix * ndc_corners.at(i);
    frustum.Corners.at(i) = Math::Vec3{world_pos / world_pos.w};
  }

  // Gribb/Hartmann method for extracting planes from the view-projection matrix
  const auto &mat = viewProjectionMatrix;

  for (int i = 0; i < 4; ++i) {
    frustum.Left[i] = mat.At(3, i) + mat.At(0, i);
    frustum.Right[i] = mat.At(3, i) - mat.At(0, i);
    frustum.Bottom[i] = mat.At(3, i) + mat.At(1, i);
    frustum.Top[i] = mat.At(3, i) - mat.At(1, i);
    frustum.Near[i] = mat.At(3, i) + mat.At(2, i);
    frustum.Far[i] = mat.At(3, i) - mat.At(2, i);
  }

  // Normalize the planes
  frustum.Left = frustum.Left.Normalize();
  frustum.Right = frustum.Right.Normalize();
  frustum.Bottom = frustum.Bottom.Normalize();
  frustum.Top = frustum.Top.Normalize();
  frustum.Near = frustum.Near.Normalize();
  frustum.Far = frustum.Far.Normalize();

  return frustum;
}

} // namespace Engine::Scene