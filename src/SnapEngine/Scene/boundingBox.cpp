#include "boundingBox.hpp"

namespace Engine {
auto BoundingBox::GetCenter() const -> Math::Vec3 {
  return (Min + Max) * 0.5F; // NOLINT
}

auto BoundingBox::GetSize() const -> Math::Vec3 { return Max - Min; }

auto BoundingBox::Union(const BoundingBox &other) const -> BoundingBox {
  BoundingBox result{};
  result.Min = Math::Min(Min, other.Min);
  result.Max = Math::Max(Max, other.Max);
  return result;
}

auto BoundingBox::UnionInPlace(const BoundingBox &other) -> void {
  Min = Math::Min(Min, other.Min);
  Max = Math::Max(Max, other.Max);
}

auto BoundingBox::Intersect(const BoundingBox &other) const -> BoundingBox {
  BoundingBox result{};
  result.Min = Math::Max(Min, other.Min);
  result.Max = Math::Min(Max, other.Max);
  return result;
}

auto BoundingBox::IntersectInPlace(const BoundingBox &other) -> void {
  Min = Math::Max(Min, other.Min);
  Max = Math::Min(Max, other.Max);
}

auto BoundingBox::IsValid() const -> bool {
  return Min.x <= Max.x && Min.y <= Max.y && Min.z <= Max.z;
}
} // namespace Engine