#pragma once

#include "Modules/Math/vector.hpp"
namespace Engine {
struct BoundingBox {
  Math::Vec3 Min;
  Math::Vec3 Max;

  [[nodiscard]] auto GetCenter() const -> Math::Vec3;
  [[nodiscard]] auto GetSize() const -> Math::Vec3;
  [[nodiscard]] auto Union(const BoundingBox &other) const -> BoundingBox;
  auto UnionInPlace(const BoundingBox &other) -> void;
  [[nodiscard]] auto Intersect(const BoundingBox &other) const -> BoundingBox;
  auto IntersectInPlace(const BoundingBox &other) -> void;
  [[nodiscard]] auto IsValid() const -> bool;
};
} // namespace Engine