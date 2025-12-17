#include "vector.hpp"
#include <cmath>
#include <cstdint>

namespace Math {

static constexpr Scalar Epsilon = 1e-6F;

///// Vec2 /////

/// Addition ///
auto Vec2::operator+(const Vec2 &other) const -> Vec2 {
  return {x + other.x, y + other.y};
}
auto Vec2::operator+(Scalar scalar) const -> Vec2 {
  return {x + scalar, y + scalar};
}
auto Vec2::operator+=(const Vec2 &other) -> Vec2 & {
  x += other.x;
  y += other.y;
  return *this;
}
auto Vec2::operator+=(Scalar scalar) -> Vec2 & {
  x += scalar;
  y += scalar;
  return *this;
}

/// Subtraction ///
auto Vec2::operator-(const Vec2 &other) const -> Vec2 {
  return {x - other.x, y - other.y};
}
auto Vec2::operator-(Scalar scalar) const -> Vec2 {
  return {x - scalar, y - scalar};
}
auto Vec2::operator-=(const Vec2 &other) -> Vec2 & {
  x -= other.x;
  y -= other.y;
  return *this;
}
auto Vec2::operator-=(Scalar scalar) -> Vec2 & {
  x -= scalar;
  y -= scalar;
  return *this;
}

/// Multiplication ///
auto Vec2::operator*(Scalar scalar) const -> Vec2 {
  return {x * scalar, y * scalar};
}
auto Vec2::operator*(const Vec2 &other) const -> Vec2 {
  return {x * other.x, y * other.y};
}
auto Vec2::operator*=(Scalar scalar) -> Vec2 & {
  x *= scalar;
  y *= scalar;
  return *this;
}
auto Vec2::operator*=(const Vec2 &other) -> Vec2 & {
  x *= other.x;
  y *= other.y;
  return *this;
}

/// Division ///
auto Vec2::operator/(Scalar scalar) const -> Vec2 {
  return {x / scalar, y / scalar};
}
auto Vec2::operator/(const Vec2 &other) const -> Vec2 {
  return {x / other.x, y / other.y};
}
auto Vec2::operator/=(Scalar scalar) -> Vec2 & {
  x /= scalar;
  y /= scalar;
  return *this;
}
auto Vec2::operator/=(const Vec2 &other) -> Vec2 & {
  x /= other.x;
  y /= other.y;
  return *this;
}

auto Vec2::operator==(const Vec2 &other) const -> bool {
  return x == other.x && y == other.y;
}
auto Vec2::operator!=(const Vec2 &other) const -> bool {
  return !(*this == other);
}
auto Vec2::Length() const -> Scalar { return std::sqrt((x * x) + (y * y)); }
auto Vec2::Inverse() const -> Vec2 { return {1.0F / x, 1.0F / y}; }
auto Vec2::Normalize() const -> Vec2 {
  Scalar len = Length();
  if (len <= Epsilon) {
    return {0.0F, 0.0F};
  }
  return {x / len, y / len};
}
auto Vec2::Dot(const Vec2 &other) const -> Scalar {
  return (x * other.x) + (y * other.y);
}
auto Vec2::Cross(const Vec2 &other) const -> Scalar {
  return (x * other.y) - (y * other.x);
}
auto Vec2::Max(const Vec2 &other) const -> Vec2 {
  return {std::fmax(x, other.x), std::fmax(y, other.y)};
}
auto Vec2::Max(Scalar scalar) const -> Vec2 {
  return {std::fmax(x, scalar), std::fmax(y, scalar)};
}
auto Vec2::Min(const Vec2 &other) const -> Vec2 {
  return {std::fmin(x, other.x), std::fmin(y, other.y)};
}
auto Vec2::Min(Scalar scalar) const -> Vec2 {
  return {std::fmin(x, scalar), std::fmin(y, scalar)};
}
Vec2::Vec2(const Vec3 &vec3) : x(vec3.x), y(vec3.y) {}
Vec2::Vec2(const Vec4 &vec4) : x(vec4.x), y(vec4.y) {}
Vec2::Vec2(const Ivec2 &vec2)
    : x(static_cast<Scalar>(vec2.x)), y(static_cast<Scalar>(vec2.y)) {}
Vec2::Vec2(const Ivec3 &vec3)
    : x(static_cast<Scalar>(vec3.x)), y(static_cast<Scalar>(vec3.y)) {}
Vec2::Vec2(const Ivec4 &vec4)
    : x(static_cast<Scalar>(vec4.x)), y(static_cast<Scalar>(vec4.y)) {}
Vec2::Vec2(const Uvec2 &vec2)
    : x(static_cast<Scalar>(vec2.x)), y(static_cast<Scalar>(vec2.y)) {}
Vec2::Vec2(const Uvec3 &vec3)
    : x(static_cast<Scalar>(vec3.x)), y(static_cast<Scalar>(vec3.y)) {}
Vec2::Vec2(const Uvec4 &vec4)
    : x(static_cast<Scalar>(vec4.x)), y(static_cast<Scalar>(vec4.y)) {}

///// Vec3 /////

/// Addition ///
auto Vec3::operator+(const Vec3 &other) const -> Vec3 {
  return {x + other.x, y + other.y, z + other.z};
}
auto Vec3::operator+(Scalar scalar) const -> Vec3 {
  return {x + scalar, y + scalar, z + scalar};
}
auto Vec3::operator+=(const Vec3 &other) -> Vec3 & {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}
auto Vec3::operator+=(Scalar scalar) -> Vec3 & {
  x += scalar;
  y += scalar;
  z += scalar;
  return *this;
}

/// Subtraction ///
auto Vec3::operator-(const Vec3 &other) const -> Vec3 {
  return {x - other.x, y - other.y, z - other.z};
}
auto Vec3::operator-(Scalar scalar) const -> Vec3 {
  return {x - scalar, y - scalar, z - scalar};
}
auto Vec3::operator-=(const Vec3 &other) -> Vec3 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}
auto Vec3::operator-=(Scalar scalar) -> Vec3 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  return *this;
}

/// Multiplication ///
auto Vec3::operator*(Scalar scalar) const -> Vec3 {
  return {x * scalar, y * scalar, z * scalar};
}
auto Vec3::operator*(const Vec3 &other) const -> Vec3 {
  return {x * other.x, y * other.y, z * other.z};
}
auto Vec3::operator*=(Scalar scalar) -> Vec3 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}
auto Vec3::operator*=(const Vec3 &other) -> Vec3 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  return *this;
}

/// Division ///
auto Vec3::operator/(Scalar scalar) const -> Vec3 {
  return {x / scalar, y / scalar, z / scalar};
}
auto Vec3::operator/(const Vec3 &other) const -> Vec3 {
  return {x / other.x, y / other.y, z / other.z};
}
auto Vec3::operator/=(Scalar scalar) -> Vec3 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  return *this;
}
auto Vec3::operator/=(const Vec3 &other) -> Vec3 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  return *this;
}

auto Vec3::operator==(const Vec3 &other) const -> bool {
  return x == other.x && y == other.y && z == other.z;
}
auto Vec3::operator!=(const Vec3 &other) const -> bool {
  return !(*this == other);
}
auto Vec3::Length() const -> Scalar {
  return std::sqrt((x * x) + (y * y) + (z * z));
}
auto Vec3::Inverse() const -> Vec3 { return {1.0F / x, 1.0F / y, 1.0F / z}; }
auto Vec3::Normalize() const -> Vec3 {
  Scalar len = Length();
  if (len <= Epsilon) {
    return {0.0F, 0.0F, 0.0F};
  }
  return {x / len, y / len, z / len};
}
auto Vec3::Dot(const Vec3 &other) const -> Scalar {
  return (x * other.x) + (y * other.y) + (z * other.z);
}
auto Vec3::Cross(const Vec3 &other) const -> Scalar {
  return (y * other.z - z * other.y) + (z * other.x - x * other.z) +
         (x * other.y - y * other.x);
}
auto Vec3::Max(const Vec3 &other) const -> Vec3 {
  return {std::fmax(x, other.x), std::fmax(y, other.y), std::fmax(z, other.z)};
}
auto Vec3::Max(Scalar scalar) const -> Vec3 {
  return {std::fmax(x, scalar), std::fmax(y, scalar), std::fmax(z, scalar)};
}
auto Vec3::Min(const Vec3 &other) const -> Vec3 {
  return {std::fmin(x, other.x), std::fmin(y, other.y), std::fmin(z, other.z)};
}
auto Vec3::Min(Scalar scalar) const -> Vec3 {
  return {std::fmin(x, scalar), std::fmin(y, scalar), std::fmin(z, scalar)};
}
Vec3::Vec3(const Vec2 &vec2, Scalar z_val) : x(vec2.x), y(vec2.y), z(z_val) {}
Vec3::Vec3(const Vec4 &vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}
Vec3::Vec3(const Ivec2 &vec2, Scalar z_val)
    : x(static_cast<Scalar>(vec2.x)), y(static_cast<Scalar>(vec2.y)), z(z_val) {
}
Vec3::Vec3(const Ivec3 &vec3)
    : x(static_cast<Scalar>(vec3.x)), y(static_cast<Scalar>(vec3.y)),
      z(static_cast<Scalar>(vec3.z)) {}
Vec3::Vec3(const Ivec4 &vec4)
    : x(static_cast<Scalar>(vec4.x)), y(static_cast<Scalar>(vec4.y)),
      z(static_cast<Scalar>(vec4.z)) {}
Vec3::Vec3(const Uvec2 &vec2, Scalar z_val)
    : x(static_cast<Scalar>(vec2.x)), y(static_cast<Scalar>(vec2.y)), z(z_val) {
}
Vec3::Vec3(const Uvec3 &vec3)
    : x(static_cast<Scalar>(vec3.x)), y(static_cast<Scalar>(vec3.y)),
      z(static_cast<Scalar>(vec3.z)) {}
Vec3::Vec3(const Uvec4 &vec4)
    : x(static_cast<Scalar>(vec4.x)), y(static_cast<Scalar>(vec4.y)),
      z(static_cast<Scalar>(vec4.z)) {}

///// Vec4 /////

/// Addition ///
auto Vec4::operator+(const Vec4 &other) const -> Vec4 {
  return {x + other.x, y + other.y, z + other.z, w + other.w};
}
auto Vec4::operator+(Scalar scalar) const -> Vec4 {
  return {x + scalar, y + scalar, z + scalar, w + scalar};
}
auto Vec4::operator+=(const Vec4 &other) -> Vec4 & {
  x += other.x;
  y += other.y;
  z += other.z;
  w += other.w;
  return *this;
}
auto Vec4::operator+=(Scalar scalar) -> Vec4 & {
  x += scalar;
  y += scalar;
  z += scalar;
  w += scalar;
  return *this;
}

/// Subtraction ///
auto Vec4::operator-(const Vec4 &other) const -> Vec4 {
  return {x - other.x, y - other.y, z - other.z, w - other.w};
}
auto Vec4::operator-(Scalar scalar) const -> Vec4 {
  return {x - scalar, y - scalar, z - scalar, w - scalar};
}
auto Vec4::operator-=(const Vec4 &other) -> Vec4 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  w -= other.w;
  return *this;
}
auto Vec4::operator-=(Scalar scalar) -> Vec4 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  w -= scalar;
  return *this;
}

/// Multiplication ///
auto Vec4::operator*(Scalar scalar) const -> Vec4 {
  return {x * scalar, y * scalar, z * scalar, w * scalar};
}
auto Vec4::operator*(const Vec4 &other) const -> Vec4 {
  return {x * other.x, y * other.y, z * other.z, w * other.w};
}
auto Vec4::operator*=(Scalar scalar) -> Vec4 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  w *= scalar;
  return *this;
}
auto Vec4::operator*=(const Vec4 &other) -> Vec4 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  w *= other.w;
  return *this;
}

/// Division ///
auto Vec4::operator/(Scalar scalar) const -> Vec4 {
  return {x / scalar, y / scalar, z / scalar, w / scalar};
}
auto Vec4::operator/(const Vec4 &other) const -> Vec4 {
  return {x / other.x, y / other.y, z / other.z, w / other.w};
}
auto Vec4::operator/=(Scalar scalar) -> Vec4 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  w /= scalar;
  return *this;
}
auto Vec4::operator/=(const Vec4 &other) -> Vec4 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  w /= other.w;
  return *this;
}

auto Vec4::operator==(const Vec4 &other) const -> bool {
  return x == other.x && y == other.y && z == other.z && w == other.w;
}
auto Vec4::operator!=(const Vec4 &other) const -> bool {
  return !(*this == other);
}
auto Vec4::Length() const -> Scalar {
  return std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
}
auto Vec4::Inverse() const -> Vec4 {
  return {1.0F / x, 1.0F / y, 1.0F / z, 1.0F / w};
}
auto Vec4::Normalize() const -> Vec4 {
  Scalar len = Length();
  if (len <= Epsilon) {
    return {0.0F, 0.0F, 0.0F, 0.0F};
  }
  return {x / len, y / len, z / len, w / len};
}
auto Vec4::Dot(const Vec4 &other) const -> Scalar {
  return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w);
}
auto Vec4::Max(const Vec4 &other) const -> Vec4 {
  return {std::fmax(x, other.x), std::fmax(y, other.y), std::fmax(z, other.z),
          std::fmax(w, other.w)};
}
auto Vec4::Max(Scalar scalar) const -> Vec4 {
  return {std::fmax(x, scalar), std::fmax(y, scalar), std::fmax(z, scalar),
          std::fmax(w, scalar)};
}
auto Vec4::Min(const Vec4 &other) const -> Vec4 {
  return {std::fmin(x, other.x), std::fmin(y, other.y), std::fmin(z, other.z),
          std::fmin(w, other.w)};
}
auto Vec4::Min(Scalar scalar) const -> Vec4 {
  return {std::fmin(x, scalar), std::fmin(y, scalar), std::fmin(z, scalar),
          std::fmin(w, scalar)};
}
Vec4::Vec4(const Vec2 &vec2, Scalar z_val, Scalar w_val)
    : x(vec2.x), y(vec2.y), z(z_val), w(w_val) {}
Vec4::Vec4(const Vec3 &vec3, Scalar w_val)
    : x(vec3.x), y(vec3.y), z(vec3.z), w(w_val) {}
Vec4::Vec4(const Ivec2 &vec2, Scalar z_val, Scalar w_val)
    : x(static_cast<Scalar>(vec2.x)), y(static_cast<Scalar>(vec2.y)), z(z_val),
      w(w_val) {}
Vec4::Vec4(const Ivec3 &vec3, Scalar w_val)
    : x(static_cast<Scalar>(vec3.x)), y(static_cast<Scalar>(vec3.y)),
      z(static_cast<Scalar>(vec3.z)), w(w_val) {}
Vec4::Vec4(const Ivec4 &vec4)
    : x(static_cast<Scalar>(vec4.x)), y(static_cast<Scalar>(vec4.y)),
      z(static_cast<Scalar>(vec4.z)), w(static_cast<Scalar>(vec4.w)) {}
Vec4::Vec4(const Uvec2 &vec2, Scalar z_val, Scalar w_val)
    : x(static_cast<Scalar>(vec2.x)), y(static_cast<Scalar>(vec2.y)), z(z_val),
      w(w_val) {}
Vec4::Vec4(const Uvec3 &vec3, Scalar w_val)
    : x(static_cast<Scalar>(vec3.x)), y(static_cast<Scalar>(vec3.y)),
      z(static_cast<Scalar>(vec3.z)), w(w_val) {}
Vec4::Vec4(const Uvec4 &vec4)
    : x(static_cast<Scalar>(vec4.x)), y(static_cast<Scalar>(vec4.y)),
      z(static_cast<Scalar>(vec4.z)), w(static_cast<Scalar>(vec4.w)) {}

///// Uvec2 /////

/// Addition ///
auto Uvec2::operator+(const Uvec2 &other) const -> Uvec2 {
  return {x + other.x, y + other.y};
}
auto Uvec2::operator+(uint32_t scalar) const -> Uvec2 {
  return {x + scalar, y + scalar};
}
auto Uvec2::operator+=(const Uvec2 &other) -> Uvec2 & {
  x += other.x;
  y += other.y;
  return *this;
}
auto Uvec2::operator+=(uint32_t scalar) -> Uvec2 & {
  x += scalar;
  y += scalar;
  return *this;
}

/// Subtraction ///
auto Uvec2::operator-(const Uvec2 &other) const -> Uvec2 {
  return {x - other.x, y - other.y};
}
auto Uvec2::operator-(uint32_t scalar) const -> Uvec2 {
  return {x - scalar, y - scalar};
}
auto Uvec2::operator-=(const Uvec2 &other) -> Uvec2 & {
  x -= other.x;
  y -= other.y;
  return *this;
}
auto Uvec2::operator-=(uint32_t scalar) -> Uvec2 & {
  x -= scalar;
  y -= scalar;
  return *this;
}

/// Multiplication ///
auto Uvec2::operator*(uint32_t scalar) const -> Uvec2 {
  return {x * scalar, y * scalar};
}
auto Uvec2::operator*(const Uvec2 &other) const -> Uvec2 {
  return {x * other.x, y * other.y};
}
auto Uvec2::operator*=(uint32_t scalar) -> Uvec2 & {
  x *= scalar;
  y *= scalar;
  return *this;
}
auto Uvec2::operator*=(const Uvec2 &other) -> Uvec2 & {
  x *= other.x;
  y *= other.y;
  return *this;
}

/// Division ///
auto Uvec2::operator/(uint32_t scalar) const -> Uvec2 {
  return {x / scalar, y / scalar};
}
auto Uvec2::operator/(const Uvec2 &other) const -> Uvec2 {
  return {x / other.x, y / other.y};
}
auto Uvec2::operator/=(uint32_t scalar) -> Uvec2 & {
  x /= scalar;
  y /= scalar;
  return *this;
}
auto Uvec2::operator/=(const Uvec2 &other) -> Uvec2 & {
  x /= other.x;
  y /= other.y;
  return *this;
}

auto Uvec2::operator==(const Uvec2 &other) const -> bool {
  return x == other.x && y == other.y;
}
auto Uvec2::operator!=(const Uvec2 &other) const -> bool {
  return !(*this == other);
}
Uvec2::Uvec2(const Uvec3 &vec3) : x(vec3.x), y(vec3.y) {}
Uvec2::Uvec2(const Uvec4 &vec4) : x(vec4.x), y(vec4.y) {}
Uvec2::Uvec2(const Ivec2 &vec2)
    : x(static_cast<uint32_t>(vec2.x)), y(static_cast<uint32_t>(vec2.y)) {}
Uvec2::Uvec2(const Ivec3 &vec3)
    : x(static_cast<uint32_t>(vec3.x)), y(static_cast<uint32_t>(vec3.y)) {}
Uvec2::Uvec2(const Ivec4 &vec4)
    : x(static_cast<uint32_t>(vec4.x)), y(static_cast<uint32_t>(vec4.y)) {}

///// Uvec3 /////

/// Addition ///
auto Uvec3::operator+(const Uvec3 &other) const -> Uvec3 {
  return {x + other.x, y + other.y, z + other.z};
}
auto Uvec3::operator+(uint32_t scalar) const -> Uvec3 {
  return {x + scalar, y + scalar, z + scalar};
}
auto Uvec3::operator+=(const Uvec3 &other) -> Uvec3 & {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}
auto Uvec3::operator+=(uint32_t scalar) -> Uvec3 & {
  x += scalar;
  y += scalar;
  z += scalar;
  return *this;
}

/// Subtraction ///
auto Uvec3::operator-(const Uvec3 &other) const -> Uvec3 {
  return {x - other.x, y - other.y, z - other.z};
}
auto Uvec3::operator-(uint32_t scalar) const -> Uvec3 {
  return {x - scalar, y - scalar, z - scalar};
}
auto Uvec3::operator-=(const Uvec3 &other) -> Uvec3 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}
auto Uvec3::operator-=(uint32_t scalar) -> Uvec3 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  return *this;
}

/// Multiplication ///
auto Uvec3::operator*(uint32_t scalar) const -> Uvec3 {
  return {x * scalar, y * scalar, z * scalar};
}
auto Uvec3::operator*(const Uvec3 &other) const -> Uvec3 {
  return {x * other.x, y * other.y, z * other.z};
}
auto Uvec3::operator*=(uint32_t scalar) -> Uvec3 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}
auto Uvec3::operator*=(const Uvec3 &other) -> Uvec3 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  return *this;
}

/// Division ///
auto Uvec3::operator/(uint32_t scalar) const -> Uvec3 {
  return {x / scalar, y / scalar, z / scalar};
}
auto Uvec3::operator/(const Uvec3 &other) const -> Uvec3 {
  return {x / other.x, y / other.y, z / other.z};
}
auto Uvec3::operator/=(uint32_t scalar) -> Uvec3 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  return *this;
}
auto Uvec3::operator/=(const Uvec3 &other) -> Uvec3 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  return *this;
}

auto Uvec3::operator==(const Uvec3 &other) const -> bool {
  return x == other.x && y == other.y && z == other.z;
}
auto Uvec3::operator!=(const Uvec3 &other) const -> bool {
  return !(*this == other);
}
Uvec3::Uvec3(const Uvec2 &vec2, uint32_t z_val)
    : x(vec2.x), y(vec2.y), z(z_val) {}
Uvec3::Uvec3(const Uvec4 &vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}
Uvec3::Uvec3(const Ivec2 &vec2, uint32_t z_val)
    : x(static_cast<uint32_t>(vec2.x)), y(static_cast<uint32_t>(vec2.y)),
      z(z_val) {}
Uvec3::Uvec3(const Ivec3 &vec3)
    : x(static_cast<uint32_t>(vec3.x)), y(static_cast<uint32_t>(vec3.y)),
      z(static_cast<uint32_t>(vec3.z)) {}
Uvec3::Uvec3(const Ivec4 &vec4)
    : x(static_cast<uint32_t>(vec4.x)), y(static_cast<uint32_t>(vec4.y)),
      z(static_cast<uint32_t>(vec4.z)) {}

///// Uvec4 /////

/// Addition ///
auto Uvec4::operator+(const Uvec4 &other) const -> Uvec4 {
  return {x + other.x, y + other.y, z + other.z, w + other.w};
}
auto Uvec4::operator+(uint32_t scalar) const -> Uvec4 {
  return {x + scalar, y + scalar, z + scalar, w + scalar};
}
auto Uvec4::operator+=(const Uvec4 &other) -> Uvec4 & {
  x += other.x;
  y += other.y;
  z += other.z;
  w += other.w;
  return *this;
}
auto Uvec4::operator+=(uint32_t scalar) -> Uvec4 & {
  x += scalar;
  y += scalar;
  z += scalar;
  w += scalar;
  return *this;
}

/// Subtraction ///
auto Uvec4::operator-(const Uvec4 &other) const -> Uvec4 {
  return {x - other.x, y - other.y, z - other.z, w - other.w};
}
auto Uvec4::operator-(uint32_t scalar) const -> Uvec4 {
  return {x - scalar, y - scalar, z - scalar, w - scalar};
}
auto Uvec4::operator-=(const Uvec4 &other) -> Uvec4 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  w -= other.w;
  return *this;
}
auto Uvec4::operator-=(uint32_t scalar) -> Uvec4 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  w -= scalar;
  return *this;
}

/// Multiplication ///
auto Uvec4::operator*(uint32_t scalar) const -> Uvec4 {
  return {x * scalar, y * scalar, z * scalar, w * scalar};
}
auto Uvec4::operator*(const Uvec4 &other) const -> Uvec4 {
  return {x * other.x, y * other.y, z * other.z, w * other.w};
}
auto Uvec4::operator*=(uint32_t scalar) -> Uvec4 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  w *= scalar;
  return *this;
}
auto Uvec4::operator*=(const Uvec4 &other) -> Uvec4 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  w *= other.w;
  return *this;
}

/// Division ///
auto Uvec4::operator/(uint32_t scalar) const -> Uvec4 {
  return {x / scalar, y / scalar, z / scalar, w / scalar};
}
auto Uvec4::operator/(const Uvec4 &other) const -> Uvec4 {
  return {x / other.x, y / other.y, z / other.z, w / other.w};
}
auto Uvec4::operator/=(uint32_t scalar) -> Uvec4 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  w /= scalar;
  return *this;
}
auto Uvec4::operator/=(const Uvec4 &other) -> Uvec4 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  w /= other.w;
  return *this;
}

auto Uvec4::operator==(const Uvec4 &other) const -> bool {
  return x == other.x && y == other.y && z == other.z && w == other.w;
}
auto Uvec4::operator!=(const Uvec4 &other) const -> bool {
  return !(*this == other);
}
Uvec4::Uvec4(const Uvec2 &vec2, uint32_t z_val, uint32_t w_val)
    : x(vec2.x), y(vec2.y), z(z_val), w(w_val) {}
Uvec4::Uvec4(const Uvec3 &vec3, uint32_t w_val)
    : x(vec3.x), y(vec3.y), z(vec3.z), w(w_val) {}
Uvec4::Uvec4(const Ivec2 &vec2, uint32_t z_val, uint32_t w_val)
    : x(static_cast<uint32_t>(vec2.x)), y(static_cast<uint32_t>(vec2.y)),
      z(z_val), w(w_val) {}
Uvec4::Uvec4(const Ivec3 &vec3, uint32_t w_val)
    : x(static_cast<uint32_t>(vec3.x)), y(static_cast<uint32_t>(vec3.y)),
      z(static_cast<uint32_t>(vec3.z)), w(w_val) {}
Uvec4::Uvec4(const Ivec4 &vec4)
    : x(static_cast<uint32_t>(vec4.x)), y(static_cast<uint32_t>(vec4.y)),
      z(static_cast<uint32_t>(vec4.z)), w(static_cast<uint32_t>(vec4.w)) {}

///// Ivec2 /////

/// Addition ///
auto Ivec2::operator+(const Ivec2 &other) const -> Ivec2 {
  return {x + other.x, y + other.y};
}
auto Ivec2::operator+(int32_t scalar) const -> Ivec2 {
  return {x + scalar, y + scalar};
}
auto Ivec2::operator+=(const Ivec2 &other) -> Ivec2 & {
  x += other.x;
  y += other.y;
  return *this;
}
auto Ivec2::operator+=(int32_t scalar) -> Ivec2 & {
  x += scalar;
  y += scalar;
  return *this;
}

/// Subtraction ///
auto Ivec2::operator-(const Ivec2 &other) const -> Ivec2 {
  return {x - other.x, y - other.y};
}
auto Ivec2::operator-(int32_t scalar) const -> Ivec2 {
  return {x - scalar, y - scalar};
}
auto Ivec2::operator-=(const Ivec2 &other) -> Ivec2 & {
  x -= other.x;
  y -= other.y;
  return *this;
}
auto Ivec2::operator-=(int32_t scalar) -> Ivec2 & {
  x -= scalar;
  y -= scalar;
  return *this;
}

/// Multiplication ///
auto Ivec2::operator*(int32_t scalar) const -> Ivec2 {
  return {x * scalar, y * scalar};
}
auto Ivec2::operator*(const Ivec2 &other) const -> Ivec2 {
  return {x * other.x, y * other.y};
}
auto Ivec2::operator*=(int32_t scalar) -> Ivec2 & {
  x *= scalar;
  y *= scalar;
  return *this;
}
auto Ivec2::operator*=(const Ivec2 &other) -> Ivec2 & {
  x *= other.x;
  y *= other.y;
  return *this;
}

/// Division ///
auto Ivec2::operator/(int32_t scalar) const -> Ivec2 {
  return {x / scalar, y / scalar};
}
auto Ivec2::operator/(const Ivec2 &other) const -> Ivec2 {
  return {x / other.x, y / other.y};
}
auto Ivec2::operator/=(int32_t scalar) -> Ivec2 & {
  x /= scalar;
  y /= scalar;
  return *this;
}
auto Ivec2::operator/=(const Ivec2 &other) -> Ivec2 & {
  x /= other.x;
  y /= other.y;
  return *this;
}

auto Ivec2::operator==(const Ivec2 &other) const -> bool {
  return x == other.x && y == other.y;
}
auto Ivec2::operator!=(const Ivec2 &other) const -> bool {
  return !(*this == other);
}
Ivec2::Ivec2(const Ivec3 &vec3) : x(vec3.x), y(vec3.y) {}
Ivec2::Ivec2(const Ivec4 &vec4) : x(vec4.x), y(vec4.y) {}
Ivec2::Ivec2(const Uvec2 &vec2)
    : x(static_cast<int32_t>(vec2.x)), y(static_cast<int32_t>(vec2.y)) {}
Ivec2::Ivec2(const Uvec3 &vec3)
    : x(static_cast<int32_t>(vec3.x)), y(static_cast<int32_t>(vec3.y)) {}
Ivec2::Ivec2(const Uvec4 &vec4)
    : x(static_cast<int32_t>(vec4.x)), y(static_cast<int32_t>(vec4.y)) {}

///// Ivec3 /////

/// Addition ///
auto Ivec3::operator+(const Ivec3 &other) const -> Ivec3 {
  return {x + other.x, y + other.y, z + other.z};
}
auto Ivec3::operator+(int32_t scalar) const -> Ivec3 {
  return {x + scalar, y + scalar, z + scalar};
}
auto Ivec3::operator+=(const Ivec3 &other) -> Ivec3 & {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}
auto Ivec3::operator+=(int32_t scalar) -> Ivec3 & {
  x += scalar;
  y += scalar;
  z += scalar;
  return *this;
}

/// Subtraction ///
auto Ivec3::operator-(const Ivec3 &other) const -> Ivec3 {
  return {x - other.x, y - other.y, z - other.z};
}
auto Ivec3::operator-(int32_t scalar) const -> Ivec3 {
  return {x - scalar, y - scalar, z - scalar};
}
auto Ivec3::operator-=(const Ivec3 &other) -> Ivec3 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}
auto Ivec3::operator-=(int32_t scalar) -> Ivec3 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  return *this;
}

/// Multiplication ///
auto Ivec3::operator*(int32_t scalar) const -> Ivec3 {
  return {x * scalar, y * scalar, z * scalar};
}
auto Ivec3::operator*(const Ivec3 &other) const -> Ivec3 {
  return {x * other.x, y * other.y, z * other.z};
}
auto Ivec3::operator*=(int32_t scalar) -> Ivec3 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}
auto Ivec3::operator*=(const Ivec3 &other) -> Ivec3 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  return *this;
}

/// Division ///
auto Ivec3::operator/(int32_t scalar) const -> Ivec3 {
  return {x / scalar, y / scalar, z / scalar};
}
auto Ivec3::operator/(const Ivec3 &other) const -> Ivec3 {
  return {x / other.x, y / other.y, z / other.z};
}
auto Ivec3::operator/=(int32_t scalar) -> Ivec3 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  return *this;
}
auto Ivec3::operator/=(const Ivec3 &other) -> Ivec3 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  return *this;
}

auto Ivec3::operator==(const Ivec3 &other) const -> bool {
  return x == other.x && y == other.y && z == other.z;
}
auto Ivec3::operator!=(const Ivec3 &other) const -> bool {
  return !(*this == other);
}
Ivec3::Ivec3(const Ivec2 &vec2, int32_t z_val)
    : x(vec2.x), y(vec2.y), z(z_val) {}
Ivec3::Ivec3(const Ivec4 &vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}
Ivec3::Ivec3(const Uvec2 &vec2, int32_t z_val)
    : x(static_cast<int32_t>(vec2.x)), y(static_cast<int32_t>(vec2.y)),
      z(z_val) {}
Ivec3::Ivec3(const Uvec3 &vec3)
    : x(static_cast<int32_t>(vec3.x)), y(static_cast<int32_t>(vec3.y)),
      z(static_cast<int32_t>(vec3.z)) {}
Ivec3::Ivec3(const Uvec4 &vec4)
    : x(static_cast<int32_t>(vec4.x)), y(static_cast<int32_t>(vec4.y)),
      z(static_cast<int32_t>(vec4.z)) {}

///// Ivec4 /////

/// Addition ///
auto Ivec4::operator+(const Ivec4 &other) const -> Ivec4 {
  return {x + other.x, y + other.y, z + other.z, w + other.w};
}
auto Ivec4::operator+(int32_t scalar) const -> Ivec4 {
  return {x + scalar, y + scalar, z + scalar, w + scalar};
}
auto Ivec4::operator+=(const Ivec4 &other) -> Ivec4 & {
  x += other.x;
  y += other.y;
  z += other.z;
  w += other.w;
  return *this;
}
auto Ivec4::operator+=(int32_t scalar) -> Ivec4 & {
  x += scalar;
  y += scalar;
  z += scalar;
  w += scalar;
  return *this;
}

/// Subtraction ///
auto Ivec4::operator-(const Ivec4 &other) const -> Ivec4 {
  return {x - other.x, y - other.y, z - other.z, w - other.w};
}
auto Ivec4::operator-(int32_t scalar) const -> Ivec4 {
  return {x - scalar, y - scalar, z - scalar, w - scalar};
}
auto Ivec4::operator-=(const Ivec4 &other) -> Ivec4 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  w -= other.w;
  return *this;
}
auto Ivec4::operator-=(int32_t scalar) -> Ivec4 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  w -= scalar;
  return *this;
}

/// Multiplication ///
auto Ivec4::operator*(int32_t scalar) const -> Ivec4 {
  return {x * scalar, y * scalar, z * scalar, w * scalar};
}
auto Ivec4::operator*(const Ivec4 &other) const -> Ivec4 {
  return {x * other.x, y * other.y, z * other.z, w * other.w};
}
auto Ivec4::operator*=(int32_t scalar) -> Ivec4 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  w *= scalar;
  return *this;
}
auto Ivec4::operator*=(const Ivec4 &other) -> Ivec4 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  w *= other.w;
  return *this;
}

/// Division ///
auto Ivec4::operator/(int32_t scalar) const -> Ivec4 {
  return {x / scalar, y / scalar, z / scalar, w / scalar};
}
auto Ivec4::operator/(const Ivec4 &other) const -> Ivec4 {
  return {x / other.x, y / other.y, z / other.z, w / other.w};
}
auto Ivec4::operator/=(int32_t scalar) -> Ivec4 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  w /= scalar;
  return *this;
}
auto Ivec4::operator/=(const Ivec4 &other) -> Ivec4 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  w /= other.w;
  return *this;
}

auto Ivec4::operator==(const Ivec4 &other) const -> bool {
  return x == other.x && y == other.y && z == other.z && w == other.w;
}
auto Ivec4::operator!=(const Ivec4 &other) const -> bool {
  return !(*this == other);
}
Ivec4::Ivec4(const Ivec2 &vec2, int32_t z_val, int32_t w_val)
    : x(vec2.x), y(vec2.y), z(z_val), w(w_val) {}
Ivec4::Ivec4(const Ivec3 &vec3, int32_t w_val)
    : x(vec3.x), y(vec3.y), z(vec3.z), w(w_val) {}
Ivec4::Ivec4(const Uvec2 &vec2, int32_t z_val, int32_t w_val)
    : x(static_cast<int32_t>(vec2.x)), y(static_cast<int32_t>(vec2.y)),
      z(z_val), w(w_val) {}
Ivec4::Ivec4(const Uvec3 &vec3, int32_t w_val)
    : x(static_cast<int32_t>(vec3.x)), y(static_cast<int32_t>(vec3.y)),
      z(static_cast<int32_t>(vec3.z)), w(w_val) {}
Ivec4::Ivec4(const Uvec4 &vec4)
    : x(static_cast<int32_t>(vec4.x)), y(static_cast<int32_t>(vec4.y)),
      z(static_cast<int32_t>(vec4.z)), w(static_cast<int32_t>(vec4.w)) {}

} // namespace Math