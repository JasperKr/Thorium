#pragma once

#include "math.hpp"
#include "vector.hpp"
namespace Math {

struct Quaternion {
  Scalar x{};
  Scalar y{};
  Scalar z{};
  Scalar w{};

  Quaternion(Scalar x_val, Scalar y_val, Scalar z_val, Scalar w_val)
      : x(x_val), y(y_val), z(z_val), w(w_val) {}

  Quaternion() : w(1.0F) {}

  [[nodiscard]] auto Normalize() const -> Quaternion;
  [[nodiscard]] auto Conjugate() const -> Quaternion;
  [[nodiscard]] auto Inverse() const -> Quaternion;
  [[nodiscard]] auto Dot(const Quaternion &other) const -> Scalar;
  [[nodiscard]] auto Multiply(const Quaternion &other) const -> Quaternion;
  [[nodiscard]] auto RotateVector(const Vec3 &vec) const -> Vec3;
};

} // namespace Math