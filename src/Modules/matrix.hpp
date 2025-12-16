#pragma once
#include "math.hpp"
#include "quaternion.hpp"
#include "vector.hpp"
#include <array>

namespace Math {

// Column-major 4x4 matrix
struct Matrix4x4 {
  constexpr static size_t Rows = 4;
  constexpr static size_t Cols = 4;
  constexpr static size_t Size = Rows * Cols;

  std::array<Scalar, Size> elements{};

  constexpr Matrix4x4(std::initializer_list<float> init) {
    std::copy(init.begin(), init.end(), elements);
  }

  static auto FromRows(std::initializer_list<float> init) -> Matrix4x4;

  auto At(size_t row, size_t col) -> Scalar &;
  auto At(size_t row, size_t col) const -> Scalar;
  auto At(size_t index) -> Scalar &;
  auto At(size_t index) const -> Scalar;

  Matrix4x4();

  explicit Matrix4x4(Quaternion quat);

  auto Transpose() -> Matrix4x4;

  auto operator*(const Matrix4x4 &other) -> Matrix4x4;
  auto operator==(const Matrix4x4 &other) const -> bool;
  auto operator!=(const Matrix4x4 &other) const -> bool;

  auto operator[](size_t index) -> Scalar &;
  auto operator[](size_t index) const -> Scalar;

  auto Determinant() const -> std::pair<Scalar, Matrix4x4>;
  auto Inverse() const -> Matrix4x4;
  auto Translate(Vec3 translation) -> Matrix4x4;
  auto Scale(Vec3 scale) -> Matrix4x4;

  auto Perspective(Scalar left, Scalar right, Scalar bottom, Scalar top,
                   Scalar nearPlane, Scalar farPlane) -> Matrix4x4;
  auto Perspective(Scalar fovRadians, Scalar aspectRatio, Scalar nearPlane,
                   Scalar farPlane) -> Matrix4x4;
  auto Orthographic(Scalar left, Scalar right, Scalar bottom, Scalar top,
                    Scalar nearPlane, Scalar farPlane) -> Matrix4x4;
  auto Orthographic(Scalar width, Scalar height, Scalar nearPlane,
                    Scalar farPlane) -> Matrix4x4;
};

} // namespace Math