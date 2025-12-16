#pragma once

#include <cmath>

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

  auto Normalize() const -> Quaternion {
    Scalar length = std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
    return {x / length, y / length, z / length, w / length};
  }

  auto Conjugate() const -> Quaternion { return {-x, -y, -z, w}; }

  auto Inverse() const -> Quaternion {
    Scalar lengthSq = (x * x) + (y * y) + (z * z) + (w * w);
    Quaternion conj = Conjugate();
    return {conj.x / lengthSq, conj.y / lengthSq, conj.z / lengthSq,
            conj.w / lengthSq};
  }

  auto Dot(const Quaternion &other) const -> Scalar {
    return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w);
  }

  auto Multiply(const Quaternion &other) const -> Quaternion {
    return {
        (w * other.x) + (x * other.w) + (y * other.z) - (z * other.y),
        (w * other.y) - (x * other.z) + (y * other.w) + (z * other.x),
        (w * other.z) - (x * other.y) + (y * other.x) + (z * other.w),
        (w * other.w) - (x * other.x) - (y * other.y) - (z * other.z),
    };
  }

  auto RotateVector(const Vec3 &vec) const -> Vec3 {
    Quaternion vecQuat{vec.x, vec.y, vec.z, 0.0F};
    Quaternion resQuat = this->Multiply(vecQuat).Multiply(this->Inverse());
    return {resQuat.x, resQuat.y, resQuat.z};
  }
};

} // namespace Math