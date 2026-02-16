#include "math.hpp"

#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include <cmath>
#include <numbers>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
namespace Math::Conversions {
// See: https://www.euclideanspace.com/maths/geometry/rotations

auto ToEuler(Quaternion quaternion) -> EulerAngle {
  // YXZ rotation order: Yaw (Y-axis), Pitch (X-axis), Roll (Z-axis)
  Scalar sqw = quaternion.w * quaternion.w;
  Scalar sqx = quaternion.x * quaternion.x;
  Scalar sqy = quaternion.y * quaternion.y;
  Scalar sqz = quaternion.z * quaternion.z;

  // Gimbal lock test: sin(pitch) = 2*(w*x - y*z)
  Scalar sinPitch =
      2.0F * ((quaternion.w * quaternion.x) - (quaternion.y * quaternion.z));
  EulerAngle result;

  if (sinPitch > 0.999F) { // singularity at north pole (pitch = +90°)
    result.pitch = std::numbers::pi_v<Scalar> / 2.0F;
    result.yaw = 2.0F * std::atan2(quaternion.y, quaternion.w);
    result.roll = 0;
    return result;
  }

  if (sinPitch < -0.999F) { // singularity at south pole (pitch = -90°)
    result.pitch = -std::numbers::pi_v<Scalar> / 2.0F;
    result.yaw = 2.0F * std::atan2(quaternion.y, quaternion.w);
    result.roll = 0;
    return result;
  }

  // General case
  result.pitch = std::asin(sinPitch);
  result.yaw = std::atan2(
      2.0F * ((quaternion.x * quaternion.z) + (quaternion.w * quaternion.y)),
      1.0F - (2.0F * (sqx + sqy)));
  result.roll = std::atan2(
      2.0F * ((quaternion.x * quaternion.y) + (quaternion.w * quaternion.z)),
      1.0F - (2.0F * (sqx + sqz)));

  return result;
}

auto ToQuaternion(EulerAngle euler) -> Quaternion {
  // YXZ rotation order: Yaw (Y-axis), Pitch (X-axis), Roll (Z-axis)
  // q = q_yaw * q_pitch * q_roll
  Scalar cos_yaw = std::cos(euler.yaw * 0.5F);
  Scalar sin_yaw = std::sin(euler.yaw * 0.5F);
  Scalar cos_pitch = std::cos(euler.pitch * 0.5F);
  Scalar sin_pitch = std::sin(euler.pitch * 0.5F);
  Scalar cos_roll = std::cos(euler.roll * 0.5F);
  Scalar sin_roll = std::sin(euler.roll * 0.5F);

  Quaternion quat;
  quat.w = (cos_yaw * cos_pitch * cos_roll) + (sin_yaw * sin_pitch * sin_roll);
  quat.x = (cos_yaw * sin_pitch * cos_roll) + (sin_yaw * cos_pitch * sin_roll);
  quat.y = (sin_yaw * cos_pitch * cos_roll) - (cos_yaw * sin_pitch * sin_roll);
  quat.z = (cos_yaw * cos_pitch * sin_roll) - (sin_yaw * sin_pitch * cos_roll);

  return quat;
}

auto ToMatrix(EulerAngle euler) -> Matrix4x4 {
  // YXZ rotation order: R = Ry(yaw) * Rx(pitch) * Rz(roll)
  // Assuming the angles are in radians.
  Scalar cos_yaw = std::cos(euler.yaw);
  Scalar sin_yaw = std::sin(euler.yaw);
  Scalar cos_pitch = std::cos(euler.pitch);
  Scalar sin_pitch = std::sin(euler.pitch);
  Scalar cos_roll = std::cos(euler.roll);
  Scalar sin_roll = std::sin(euler.roll);

  Matrix4x4 mat{}; // Identity

  mat.At(0, 0) = (cos_yaw * cos_roll) + (sin_yaw * sin_pitch * sin_roll);
  mat.At(0, 1) = (-cos_yaw * sin_roll) + (sin_yaw * sin_pitch * cos_roll);
  mat.At(0, 2) = sin_yaw * cos_pitch;

  mat.At(1, 0) = cos_pitch * sin_roll;
  mat.At(1, 1) = cos_pitch * cos_roll;
  mat.At(1, 2) = -sin_pitch;

  mat.At(2, 0) = (-sin_yaw * cos_roll) + (cos_yaw * sin_pitch * sin_roll);
  mat.At(2, 1) = (sin_yaw * sin_roll) + (cos_yaw * sin_pitch * cos_roll);
  mat.At(2, 2) = cos_yaw * cos_pitch;

  return mat;
}

auto ToEuler(Matrix4x4 matrix) -> EulerAngle {
  // YXZ rotation order extraction
  // mat[1][2] = -sin(pitch), mat[1][0] = cos(pitch)*sin(roll),
  // mat[1][1] = cos(pitch)*cos(roll), mat[0][2] = sin(yaw)*cos(pitch),
  // mat[2][2] = cos(yaw)*cos(pitch)
  EulerAngle result;

  Scalar sinPitch = -matrix.At(1, 2);

  if (sinPitch > 0.999F) { // singularity at north pole (pitch = +90°)
    result.pitch = std::numbers::pi_v<Scalar> / 2.0F;
    result.yaw = std::atan2(matrix.At(0, 1), matrix.At(0, 0));
    result.roll = 0;
    return result;
  }

  if (sinPitch < -0.999F) { // singularity at south pole (pitch = -90°)
    result.pitch = -std::numbers::pi_v<Scalar> / 2.0F;
    result.yaw = std::atan2(-matrix.At(0, 1), matrix.At(0, 0));
    result.roll = 0;
    return result;
  }

  result.pitch = std::asin(sinPitch);
  result.yaw = std::atan2(matrix.At(0, 2), matrix.At(2, 2));
  result.roll = std::atan2(matrix.At(1, 0), matrix.At(1, 1));

  return result;
}

auto ToQuaternion(Matrix4x4 matrix) -> Quaternion {
  Quaternion quat;

  Scalar trace = matrix.At(0, 0) + matrix.At(1, 1) +
                 matrix.At(2, 2); // I removed + 1.0f; see discussion with Ethan
  if (trace > 0) {                // I changed M_EPSILON to 0
    Scalar scale = 0.5F / std::sqrt(trace + 1.0F);
    quat.w = 0.25F / scale;
    quat.x = (matrix.At(2, 1) - matrix.At(1, 2)) * scale;
    quat.y = (matrix.At(0, 2) - matrix.At(2, 0)) * scale;
    quat.z = (matrix.At(1, 0) - matrix.At(0, 1)) * scale;
  } else {
    if (matrix.At(0, 0) > matrix.At(1, 1) &&
        matrix.At(0, 0) > matrix.At(2, 2)) {
      Scalar scale = 2.0F * std::sqrt(1.0F + matrix.At(0, 0) - matrix.At(1, 1) -
                                      matrix.At(2, 2));
      quat.w = (matrix.At(2, 1) - matrix.At(1, 2)) / scale;
      quat.x = 0.25F * scale;
      quat.y = (matrix.At(0, 1) + matrix.At(1, 0)) / scale;
      quat.z = (matrix.At(0, 2) + matrix.At(2, 0)) / scale;
    } else if (matrix.At(1, 1) > matrix.At(2, 2)) {
      Scalar scale = 2.0F * std::sqrt(1.0F + matrix.At(1, 1) - matrix.At(0, 0) -
                                      matrix.At(2, 2));
      quat.w = (matrix.At(0, 2) - matrix.At(2, 0)) / scale;
      quat.x = (matrix.At(0, 1) + matrix.At(1, 0)) / scale;
      quat.y = 0.25F * scale;
      quat.z = (matrix.At(1, 2) + matrix.At(2, 1)) / scale;
    } else {
      Scalar scale = 2.0F * std::sqrt(1.0F + matrix.At(2, 2) - matrix.At(0, 0) -
                                      matrix.At(1, 1));
      quat.w = (matrix.At(1, 0) - matrix.At(0, 1)) / scale;
      quat.x = (matrix.At(0, 2) + matrix.At(2, 0)) / scale;
      quat.y = (matrix.At(1, 2) + matrix.At(2, 1)) / scale;
      quat.z = 0.25F * scale;
    }
  }

  return quat;
}

auto ToMatrix(Quaternion quat) -> Matrix4x4 {
  Scalar sqw = quat.w * quat.w;
  Scalar sqx = quat.x * quat.x;
  Scalar sqy = quat.y * quat.y;
  Scalar sqz = quat.z * quat.z;

  Matrix4x4 mat{}; // Identity

  // invs (inverse square length) is only required if quaternion is not already normalised
  Scalar invs = 1.0F / (sqx + sqy + sqz + sqw);

  // since sqw + sqx + sqy + sqz =1/invs*invs
  mat.At(0, 0) = (sqx - sqy - sqz + sqw) * invs;
  mat.At(1, 1) = (-sqx + sqy - sqz + sqw) * invs;
  mat.At(2, 2) = (-sqx - sqy + sqz + sqw) * invs;

  Scalar tmp1 = quat.x * quat.y;
  Scalar tmp2 = quat.z * quat.w;
  mat.At(1, 0) = 2.0F * (tmp1 + tmp2) * invs;
  mat.At(0, 1) = 2.0F * (tmp1 - tmp2) * invs;

  tmp1 = quat.x * quat.z;
  tmp2 = quat.y * quat.w;
  mat.At(2, 0) = 2.0F * (tmp1 - tmp2) * invs;
  mat.At(0, 2) = 2.0F * (tmp1 + tmp2) * invs;
  tmp1 = quat.y * quat.z;
  tmp2 = quat.x * quat.w;
  mat.At(2, 1) = 2.0F * (tmp1 + tmp2) * invs;
  mat.At(1, 2) = 2.0F * (tmp1 - tmp2) * invs;

  return mat;
}

} // namespace Math::Conversions

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
