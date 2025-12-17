#pragma once
#include "math.hpp"
#include "quaternion.hpp"
#include "vector.hpp"
#include <array>
#include <span>

namespace Math {

// Column-major 4x4 matrix
struct Matrix4x4 {
  constexpr static size_t Rows = 4;
  constexpr static size_t Cols = 4;
  constexpr static size_t Size = Rows * Cols;

  std::array<Scalar, Size> elements{};

  constexpr Matrix4x4(std::initializer_list<Scalar> init) {
    size_t index = 0;
    for (const auto &value : init) {
      elements.at(index++) = value;
    }
  }

  auto AsSpan() -> std::span<Scalar> { return {elements}; }

  static auto FromRows(std::initializer_list<Scalar> init) -> Matrix4x4;

  auto At(size_t row, size_t col) -> Scalar &;
  [[nodiscard]] auto At(size_t row, size_t col) const -> Scalar;
  auto At(size_t index) -> Scalar &;
  [[nodiscard]] auto At(size_t index) const -> Scalar;

  Matrix4x4();

  explicit Matrix4x4(Quaternion quat);

  auto Transpose() -> Matrix4x4;

  auto operator*(const Matrix4x4 &other) -> Matrix4x4;
  auto operator==(const Matrix4x4 &other) const -> bool;
  auto operator!=(const Matrix4x4 &other) const -> bool;

  auto operator[](size_t index) -> Scalar &;
  auto operator[](size_t index) const -> Scalar;

  [[nodiscard]] auto Determinant() const -> std::pair<Scalar, Matrix4x4>;
  [[nodiscard]] auto Inverse() const -> Matrix4x4;
  auto Translate(Vec3 translation) -> Matrix4x4;
  auto Scale(Vec3 scale) -> Matrix4x4;

  static auto Perspective(Scalar left, Scalar right, Scalar bottom, Scalar top,
                          Scalar nearPlane, Scalar farPlane) -> Matrix4x4;
  static auto Perspective(Scalar fovRadians, Scalar aspectRatio,
                          Scalar nearPlane, Scalar farPlane) -> Matrix4x4;
  static auto Orthographic(Scalar left, Scalar right, Scalar bottom, Scalar top,
                           Scalar nearPlane, Scalar farPlane) -> Matrix4x4;
  static auto Orthographic(Scalar width, Scalar height, Scalar nearPlane,
                           Scalar farPlane) -> Matrix4x4;
  static auto TranslationMatrix(Vec3 translation) -> Matrix4x4;
  static auto ScaleMatrix(Vec3 scale) -> Matrix4x4;
  static auto RotationMatrix(Quaternion rotation) -> Matrix4x4;
  static auto TransformationMatrix(Vec3 translation, Vec3 scale,
                                   Quaternion rotation) -> Matrix4x4;
};

} // namespace Math