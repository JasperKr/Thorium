#pragma once
#include "mathTypes.hpp"
#include "quaternion.hpp"
#include "vector.hpp"
#include <array>
#include <span>
#include <string>

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

  auto AsSpan() -> std::span<const Scalar> { return {elements}; }
  auto AsByteSpan()
      -> std::span<const uint8_t> { // NOLINTNEXTLINE reinterpret cast
    return {reinterpret_cast<const uint8_t *>(elements.data()),
            sizeof(Scalar) * Size};
  }

  static auto FromRows(std::initializer_list<Scalar> init) -> Matrix4x4;

  auto At(size_t row, size_t col) -> Scalar &;
  [[nodiscard]] auto At(size_t row, size_t col) const -> Scalar;
  auto At(size_t index) -> Scalar &;
  [[nodiscard]] auto At(size_t index) const -> Scalar;

  Matrix4x4();

  // Same as default constructor, but explicitly named for clarity.
  static auto Identity() -> Matrix4x4 { return {}; }

  static auto Zero() -> Matrix4x4 {
    Matrix4x4 result{};
    for (size_t i = 0; i < Size; ++i) {
      result.elements.at(i) = 0.0F;
    }
    return result;
  }

  auto Transpose() -> Matrix4x4;

  auto operator*(const Matrix4x4 &other) const -> Matrix4x4;
  auto operator*(const Vec4 &vec) const -> Vec4;
  auto operator==(const Matrix4x4 &other) const -> bool;
  auto operator!=(const Matrix4x4 &other) const -> bool;

  auto operator[](size_t index) -> Scalar &;
  auto operator[](size_t index) const -> Scalar;

  [[nodiscard]] auto Determinant() const -> std::pair<Scalar, Matrix4x4>;
  [[nodiscard]] auto Inverse() const -> Matrix4x4;
  [[nodiscard]] auto InverseTranspose() const -> Matrix4x4;

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

  [[nodiscard]] auto ToString() const -> std::string;

  [[nodiscard]] auto data() const -> const Scalar * { return elements.data(); }
  static auto size() -> size_t { return Size; }
  [[nodiscard]] auto byteSpan() const
      -> std::span<const uint8_t> { // NOLINTNEXTLINE reinterpret cast
    return {reinterpret_cast<const uint8_t *>(elements.data()),
            sizeof(Scalar) * Size};
  }
};

struct Matrix3x3 {
  constexpr static size_t Rows = 3;
  constexpr static size_t Cols = 3;
  constexpr static size_t Size = Rows * Cols;

  std::array<Scalar, Size> elements{};

  auto At(size_t row, size_t col) -> Scalar &;
  [[nodiscard]] auto At(size_t row, size_t col) const -> Scalar;
  auto At(size_t index) -> Scalar &;
  [[nodiscard]] auto At(size_t index) const -> Scalar;

  Matrix3x3();
  Matrix3x3(std::initializer_list<Scalar> init) {
    size_t index = 0;
    for (const auto &value : init) {
      elements.at(index++) = value;
    }
  }
  explicit Matrix3x3(const Matrix4x4 &matrix4x4) {
    for (size_t row = 0; row < Rows; ++row) {
      for (size_t col = 0; col < Cols; ++col) {
        At(row, col) = matrix4x4.At(row, col);
      }
    }
  }

  static auto Identity() -> Matrix3x3 { return {}; }

  auto Transpose() -> Matrix3x3;
  auto operator*(const Matrix3x3 &other) const -> Matrix3x3;
  auto operator*(const Vec3 &vec) const -> Vec3;
  auto operator==(const Matrix3x3 &other) const -> bool;
  auto operator!=(const Matrix3x3 &other) const -> bool;

  auto operator[](size_t index) -> Scalar &;
  auto operator[](size_t index) const -> Scalar;

  [[nodiscard]] auto Determinant() const -> std::pair<Scalar, Matrix3x3>;
  [[nodiscard]] auto Inverse() const -> Matrix3x3;
  [[nodiscard]] auto InverseTranspose() const -> Matrix3x3;

  [[nodiscard]] auto ToString() const -> std::string;

  [[nodiscard]] auto data() const -> const Scalar * { return elements.data(); }
  static auto size() -> size_t { return Size; }

  [[nodiscard]] auto byteSpan() const
      -> std::span<const uint8_t> { // NOLINTNEXTLINE reinterpret cast
    return {reinterpret_cast<const uint8_t *>(elements.data()),
            sizeof(Scalar) * Size};
  }
};

} // namespace Math