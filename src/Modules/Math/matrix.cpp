#include "matrix.hpp"
#include <cmath>

#ifndef NDEBUG
#include <stdexcept>
#endif

namespace Math {

auto Matrix4x4::FromRows(std::initializer_list<Scalar> init) -> Matrix4x4 {
  return Matrix4x4(init).Transpose();
}

auto Matrix4x4::At(size_t row, size_t col) -> Scalar & {
#ifndef NDEBUG
  if (row >= 4 || col >= 4) {
    throw std::out_of_range("Matrix4x4::At - Index out of range");
  }
#endif
  return elements.at((row * Cols) + col);
}

auto Matrix4x4::At(size_t row, size_t col) const -> Scalar {
#ifndef NDEBUG
  if (row >= 4 || col >= 4) {
    throw std::out_of_range("Matrix4x4::At - Index out of range");
  }
#endif
  return elements.at((row * Cols) + col);
}

auto Matrix4x4::At(size_t index) -> Scalar & {
#ifndef NDEBUG
  if (index >= Size) {
    throw std::out_of_range("Matrix4x4::At - Index out of range");
  }
#endif
  return elements.at(index);
}

auto Matrix4x4::At(size_t index) const -> Scalar {
#ifndef NDEBUG
  if (index >= Size) {
    throw std::out_of_range("Matrix4x4::At - Index out of range");
  }
#endif
  return elements.at(index);
}

Matrix4x4::Matrix4x4() {
  At(0) = 1.0F;
  At(1, 1) = 1.0F;
  At(2, 2) = 1.0F;
  At(3, 3) = 1.0F;
}

Matrix4x4::Matrix4x4(Quaternion quat) {
  Quaternion nQuat = quat.Normalize();
  Scalar xMulX = nQuat.x * nQuat.x;
  Scalar yMulY = nQuat.y * nQuat.y;
  Scalar zMulZ = nQuat.z * nQuat.z;
  Scalar xMulY = nQuat.x * nQuat.y;
  Scalar xMulZ = nQuat.x * nQuat.z;
  Scalar yMulZ = nQuat.y * nQuat.z;
  Scalar wMulX = nQuat.w * nQuat.x;
  Scalar wMulY = nQuat.w * nQuat.y;
  Scalar wMulZ = nQuat.w * nQuat.z;

  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

  At(0, 0) = 1.0F - (2.0F * (yMulY + zMulZ));
  At(0, 1) = 2.0F * (xMulY - wMulZ);
  At(0, 2) = 2.0F * (xMulZ + wMulY);
  At(0, 3) = 0.0F;

  At(1, 0) = 2.0F * (xMulY + wMulZ);
  At(1, 1) = 1.0F - (2.0F * (xMulX + zMulZ));
  At(1, 2) = 2.0F * (yMulZ - wMulX);
  At(1, 3) = 0.0F;

  At(2, 0) = 2.0F * (xMulZ - wMulY);
  At(2, 1) = 2.0F * (yMulZ + wMulX);
  At(2, 2) = 1.0F - (2.0F * (xMulX + yMulY));
  At(2, 3) = 0.0F;

  At(3, 0) = 0.0F;
  At(3, 1) = 0.0F;
  At(3, 2) = 0.0F;
  At(3, 3) = 1.0F;

  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
}

auto Matrix4x4::Determinant() const -> std::pair<Scalar, Matrix4x4> {
  Matrix4x4 inv;

  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

  inv[0] = (At(5) * At(10) * At(15)) - (At(5) * At(11) * At(14)) -
           (At(9) * At(6) * At(15)) + (At(9) * At(7) * At(14)) +
           (At(13) * At(6) * At(11)) - (At(13) * At(7) * At(10));

  inv[4] = (-At(4) * At(10) * At(15)) + (At(4) * At(11) * At(14)) +
           (At(8) * At(6) * At(15)) - (At(8) * At(7) * At(14)) -
           (At(12) * At(6) * At(11)) + (At(12) * At(7) * At(10));

  inv[8] = (At(4) * At(9) * At(15)) - (At(4) * At(11) * At(13)) -
           (At(8) * At(5) * At(15)) + (At(8) * At(7) * At(13)) +
           (At(12) * At(5) * At(11)) - (At(12) * At(7) * At(9));

  inv[12] = (-At(4) * At(9) * At(14)) + (At(4) * At(10) * At(13)) +
            (At(8) * At(5) * At(14)) - (At(8) * At(6) * At(13)) -
            (At(12) * At(5) * At(10)) + (At(12) * At(6) * At(9));

  inv[1] = (-At(1) * At(10) * At(15)) + (At(1) * At(11) * At(14)) +
           (At(9) * At(2) * At(15)) - (At(9) * At(3) * At(14)) -
           (At(13) * At(2) * At(11)) + (At(13) * At(3) * At(10));

  inv[5] = (At(0) * At(10) * At(15)) - (At(0) * At(11) * At(14)) -
           (At(8) * At(2) * At(15)) + (At(8) * At(3) * At(14)) +
           (At(12) * At(2) * At(11)) - (At(12) * At(3) * At(10));

  inv[9] = (-At(0) * At(9) * At(15)) + (At(0) * At(11) * At(13)) +
           (At(8) * At(1) * At(15)) - (At(8) * At(3) * At(13)) -
           (At(12) * At(1) * At(11)) + (At(12) * At(3) * At(9));

  inv[13] = (At(0) * At(9) * At(14)) - (At(0) * At(10) * At(13)) -
            (At(8) * At(1) * At(14)) + (At(8) * At(2) * At(13)) +
            (At(12) * At(1) * At(10)) - (At(12) * At(2) * At(9));

  inv[2] = (At(1) * At(6) * At(15)) - (At(1) * At(7) * At(14)) -
           (At(5) * At(2) * At(15)) + (At(5) * At(3) * At(14)) +
           (At(13) * At(2) * At(7)) - (At(13) * At(3) * At(6));

  inv[6] = (-At(0) * At(6) * At(15)) + (At(0) * At(7) * At(14)) +
           (At(4) * At(2) * At(15)) - (At(4) * At(3) * At(14)) -
           (At(12) * At(2) * At(7)) + (At(12) * At(3) * At(6));

  inv[10] = (At(0) * At(5) * At(15)) - (At(0) * At(7) * At(13)) -
            (At(4) * At(1) * At(15)) + (At(4) * At(3) * At(13)) +
            (At(12) * At(1) * At(7)) - (At(12) * At(3) * At(5));

  inv[14] = (-At(0) * At(5) * At(14)) + (At(0) * At(6) * At(13)) +
            (At(4) * At(1) * At(14)) - (At(4) * At(2) * At(13)) -
            (At(12) * At(1) * At(6)) + (At(12) * At(2) * At(5));

  inv[3] = (-At(1) * At(6) * At(11)) + (At(1) * At(7) * At(10)) +
           (At(5) * At(2) * At(11)) - (At(5) * At(3) * At(10)) -
           (At(9) * At(2) * At(7)) + (At(9) * At(3) * At(6));

  inv[7] = (At(0) * At(6) * At(11)) - (At(0) * At(7) * At(10)) -
           (At(4) * At(2) * At(11)) + (At(4) * At(3) * At(10)) +
           (At(8) * At(2) * At(7)) - (At(8) * At(3) * At(6));

  inv[11] = (-At(0) * At(5) * At(11)) + (At(0) * At(7) * At(9)) +
            (At(4) * At(1) * At(11)) - (At(4) * At(3) * At(9)) -
            (At(8) * At(1) * At(7)) + (At(8) * At(3) * At(5));

  inv[15] = (At(0) * At(5) * At(10)) - (At(0) * At(6) * At(9)) -
            (At(4) * At(1) * At(10)) + (At(4) * At(2) * At(9)) +
            (At(8) * At(1) * At(6)) - ((At(8) * At(2) * At(5)));

  Scalar det = (At(0) * inv[0]) + (At(1) * inv[4]) + (At(2) * inv[8]) +
               (At(3) * inv[12]);

  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

  return {det, inv};
}

auto Matrix4x4::Transpose() -> Matrix4x4 {
  Matrix4x4 result{};
  for (size_t row = 0; row < Rows; ++row) {
    for (size_t col = 0; col < Cols; ++col) {
      // NOLINTNEXTLINE, clangd thinks row, col are swapped, but that's intentional
      result.At(col, row) = At(row, col);
    }
  }
  return result;
}

auto Matrix4x4::operator*(const Matrix4x4 &other) -> Matrix4x4 {
  Matrix4x4 result{};
  for (size_t row = 0; row < Rows; ++row) {
    for (size_t col = 0; col < Cols; ++col) {
      Scalar sum = 0.0F;
      for (size_t k = 0; k < Cols; ++k) {
        sum += At(row, k) * other.At(k, col);
      }
      result.At(row, col) = sum;
    }
  }
  return result;
}

auto Matrix4x4::operator==(const Matrix4x4 &other) const -> bool {
  for (size_t i = 0; i < Size; ++i) {
    if (this->elements.at(i) != other.elements.at(i)) {
      return false;
    }
  }
  return true;
}

auto Matrix4x4::Inverse() const -> Matrix4x4 {
  auto detPair = Determinant();
  auto det = detPair.first;
  auto inv = detPair.second;

  if (det == 0.0F) {
    return Matrix4x4{NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN,
                     NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  }

  auto invDet = 1.0F / det;

  Matrix4x4 result;

  for (size_t i = 0; i < Size; ++i) {
    result.At(i) = inv.At(i) * invDet;
  }

  return result;
}

auto Matrix4x4::operator!=(const Matrix4x4 &other) const -> bool {
  return !(*this == other);
}

auto Matrix4x4::operator[](size_t index) -> Scalar & {
#ifndef NDEBUG
  if (index >= Size) {
    throw std::out_of_range("Matrix4x4::operator[] - Index out of range");
  }
#endif
  return elements.at(index);
}

auto Matrix4x4::operator[](size_t index) const -> Scalar {
#ifndef NDEBUG
  if (index >= Size) {
    throw std::out_of_range("Matrix4x4::operator[] - Index out of range");
  }
#endif
  return elements.at(index);
}

auto Matrix4x4::Translate(Vec3 translation) -> Matrix4x4 {
  Matrix4x4 result = *this;
  result.At(0, 3) += translation.x;
  result.At(1, 3) += translation.y;
  result.At(2, 3) += translation.z;
  return result;
}

auto Matrix4x4::Scale(Vec3 scale) -> Matrix4x4 {
  Matrix4x4 result = *this;
  result.At(0, 0) *= scale.x;
  result.At(1, 1) *= scale.y;
  result.At(2, 2) *= scale.z;
  return result;
}

auto Matrix4x4::Perspective(Scalar left, Scalar right, Scalar bottom,
                            Scalar top, Scalar nearPlane, Scalar farPlane)
    -> Matrix4x4 {

  // NOLINTBEGIN
  Matrix4x4 result = Matrix4x4::FromRows(
      {(2.0F * nearPlane) / (right - left), 0.0F,
       (right + left) / (right - left), 0.0F, 0.0F,
       (2.0F * nearPlane) / (top - bottom), (top + bottom) / (top - bottom),
       0.0F, 0.0F, 0.0F, -(farPlane + nearPlane) / (farPlane - nearPlane),
       -(2.0F * farPlane * nearPlane) / (farPlane - nearPlane), 0.0F, 0.0F,
       -1.0F, 0.0F});
  // NOLINTEND

  return result;
}

// NOLINTNEXTLINE
auto Matrix4x4::Perspective(Scalar fovRadians, Scalar aspectRatio,
                            Scalar nearPlane, Scalar farPlane) -> Matrix4x4 {
  Scalar invTanHalfFov = 1.0F / std::tan(fovRadians / 2.0F); // NOLINT
  Scalar rangeInv = 1.0F / (nearPlane - farPlane);

  //NOLINTBEGIN
  Matrix4x4 result = Matrix4x4::FromRows(
      {invTanHalfFov / aspectRatio, 0.0F, 0.0F, 0.0F, 0.0F, invTanHalfFov, 0.0F,
       0.0F, 0.0F, 0.0F, (nearPlane + farPlane) * rangeInv,
       (2.0F * nearPlane * farPlane) * rangeInv, 0.0F, 0.0F, -1.0F, 0.0F});
  //NOLINTEND

  return result;
}

auto Matrix4x4::Orthographic(Scalar left, Scalar right, Scalar bottom,
                             Scalar top, Scalar nearPlane, Scalar farPlane)
    -> Matrix4x4 {
  // NOLINTBEGIN
  Matrix4x4 result = Matrix4x4::FromRows(
      {2.0F / (right - left), 0.0F, 0.0F, -(right + left) / (right - left),
       0.0F, 2.0F / (top - bottom), 0.0F, -(top + bottom) / (top - bottom),
       0.0F, 0.0F, -2.0F / (farPlane - nearPlane),
       -(farPlane + nearPlane) / (farPlane - nearPlane), 0.0F, 0.0F, 0.0F,
       1.0F});
  // NOLINTEND

  return result;
}

// NOLINTBEGIN
auto Matrix4x4::Orthographic(Scalar width, Scalar height, Scalar nearPlane,
                             Scalar farPlane) -> Matrix4x4 {
  Scalar left = -width / 2.0F;
  Scalar right = width / 2.0F;
  Scalar bottom = -height / 2.0F;
  Scalar top = height / 2.0F;

  return Matrix4x4::Orthographic(left, right, bottom, top, nearPlane, farPlane);
}
// NOLINTEND

auto Matrix4x4::TranslationMatrix(Vec3 translation) -> Matrix4x4 {
  Matrix4x4 result;
  result.At(0, 3) = translation.x;
  result.At(1, 3) = translation.y;
  result.At(2, 3) = translation.z;
  return result;
}
auto Matrix4x4::ScaleMatrix(Vec3 scale) -> Matrix4x4 {
  Matrix4x4 result;
  result.At(0, 0) = scale.x;
  result.At(1, 1) = scale.y;
  result.At(2, 2) = scale.z;
  return result;
}
auto Matrix4x4::RotationMatrix(Quaternion rotation) -> Matrix4x4 {
  return Matrix4x4(rotation);
}

// NOLINTNEXTLINE
auto Matrix4x4::TransformationMatrix(Vec3 translation, Vec3 scale,
                                     Quaternion rotation) -> Matrix4x4 {
  Matrix4x4 translationMatrix = Matrix4x4::TranslationMatrix(translation);
  Matrix4x4 scaleMatrix = Matrix4x4::ScaleMatrix(scale);
  Matrix4x4 rotationMatrix = Matrix4x4::RotationMatrix(rotation);
  return translationMatrix * rotationMatrix * scaleMatrix;
}

auto Matrix4x4::ToString() const -> std::string {
  std::string result;
  for (size_t row = 0; row < Rows; ++row) {
    result += "| ";
    for (size_t col = 0; col < Cols; ++col) {
      result += std::to_string(At(row, col)) + " ";
    }
    result += "|";
    if (row < Rows - 1) {
      result += "\n";
    }
  }
  return result;
}

} // namespace Math