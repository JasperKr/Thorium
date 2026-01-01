#pragma once

#include "math.hpp"
#include <cstdint>

namespace Math {

struct Vec2;
struct Vec3;
struct Vec4;

struct Uvec2;
struct Uvec3;
struct Uvec4;

struct Ivec2;
struct Ivec3;
struct Ivec4;

struct Vec2 {
  Scalar x{};
  Scalar y{};

  auto operator+(const Vec2 &other) const -> Vec2;
  auto operator+(Scalar scalar) const -> Vec2;
  auto operator+=(const Vec2 &other) -> Vec2 &;
  auto operator+=(Scalar scalar) -> Vec2 &;

  auto operator-(const Vec2 &other) const -> Vec2;
  auto operator-(Scalar scalar) const -> Vec2;
  auto operator-=(const Vec2 &other) -> Vec2 &;
  auto operator-=(Scalar scalar) -> Vec2 &;

  auto operator*(Scalar scalar) const -> Vec2;
  auto operator*(const Vec2 &other) const -> Vec2;
  auto operator*=(Scalar scalar) -> Vec2 &;
  auto operator*=(const Vec2 &other) -> Vec2 &;

  auto operator/(Scalar scalar) const -> Vec2;
  auto operator/(const Vec2 &other) const -> Vec2;
  auto operator/=(Scalar scalar) -> Vec2 &;
  auto operator/=(const Vec2 &other) -> Vec2 &;

  auto operator==(const Vec2 &other) const -> bool;
  auto operator!=(const Vec2 &other) const -> bool;

  auto operator[](uint32_t index) const -> Scalar;

  [[nodiscard]] auto Length() const -> Scalar;
  [[nodiscard]] auto Inverse() const -> Vec2;
  [[nodiscard]] auto Normalize() const -> Vec2;
  [[nodiscard]] auto Dot(const Vec2 &other) const -> Scalar;
  [[nodiscard]] auto Cross(const Vec2 &other) const -> Scalar;

  [[nodiscard]] auto Max(const Vec2 &other) const -> Vec2;
  [[nodiscard]] auto Max(Scalar scalar) const -> Vec2;
  [[nodiscard]] auto Min(const Vec2 &other) const -> Vec2;
  [[nodiscard]] auto Min(Scalar scalar) const -> Vec2;

  Vec2(Scalar x_val, Scalar y_val) : x(x_val), y(y_val) {}
  Vec2() = default;
  explicit Vec2(const Vec3 &vec3);
  explicit Vec2(const Vec4 &vec4);

  explicit Vec2(const Ivec2 &vec2);
  explicit Vec2(const Ivec3 &vec3);
  explicit Vec2(const Ivec4 &vec4);

  explicit Vec2(const Uvec2 &vec2);
  explicit Vec2(const Uvec3 &vec3);
  explicit Vec2(const Uvec4 &vec4);
};

struct Vec3 {
  Scalar x;
  Scalar y;
  Scalar z;

  auto operator+(const Vec3 &other) const -> Vec3;
  auto operator+(Scalar scalar) const -> Vec3;
  auto operator+=(const Vec3 &other) -> Vec3 &;
  auto operator+=(Scalar scalar) -> Vec3 &;

  auto operator-(const Vec3 &other) const -> Vec3;
  auto operator-(Scalar scalar) const -> Vec3;
  auto operator-=(const Vec3 &other) -> Vec3 &;
  auto operator-=(Scalar scalar) -> Vec3 &;

  auto operator*(Scalar scalar) const -> Vec3;
  auto operator*(const Vec3 &other) const -> Vec3;
  auto operator*=(Scalar scalar) -> Vec3 &;
  auto operator*=(const Vec3 &other) -> Vec3 &;

  auto operator/(Scalar scalar) const -> Vec3;
  auto operator/(const Vec3 &other) const -> Vec3;
  auto operator/=(Scalar scalar) -> Vec3 &;
  auto operator/=(const Vec3 &other) -> Vec3 &;

  auto operator==(const Vec3 &other) const -> bool;
  auto operator!=(const Vec3 &other) const -> bool;

  auto operator[](uint32_t index) const -> Scalar;

  [[nodiscard]] auto Length() const -> Scalar;
  [[nodiscard]] auto Inverse() const -> Vec3;
  [[nodiscard]] auto Normalize() const -> Vec3;
  [[nodiscard]] auto Dot(const Vec3 &other) const -> Scalar;
  [[nodiscard]] auto Cross(const Vec3 &other) const -> Scalar;

  [[nodiscard]] auto Max(const Vec3 &other) const -> Vec3;
  [[nodiscard]] auto Max(Scalar scalar) const -> Vec3;
  [[nodiscard]] auto Min(const Vec3 &other) const -> Vec3;
  [[nodiscard]] auto Min(Scalar scalar) const -> Vec3;

  Vec3(Scalar x_val, Scalar y_val, Scalar z_val)
      : x(x_val), y(y_val), z(z_val) {}
  Vec3() = default;
  explicit Vec3(const Vec2 &vec2, Scalar z_val = 0.0F);
  explicit Vec3(const Vec4 &vec4);

  explicit Vec3(const Ivec2 &vec2, Scalar z_val = 0.0F);
  explicit Vec3(const Ivec3 &vec3);
  explicit Vec3(const Ivec4 &vec4);

  explicit Vec3(const Uvec2 &vec2, Scalar z_val = 0.0F);
  explicit Vec3(const Uvec3 &vec3);
  explicit Vec3(const Uvec4 &vec4);
};

struct Vec4 {
  Scalar x;
  Scalar y;
  Scalar z;
  Scalar w;

  auto operator+(const Vec4 &other) const -> Vec4;
  auto operator+(Scalar scalar) const -> Vec4;
  auto operator+=(const Vec4 &other) -> Vec4 &;
  auto operator+=(Scalar scalar) -> Vec4 &;

  auto operator-(const Vec4 &other) const -> Vec4;
  auto operator-(Scalar scalar) const -> Vec4;
  auto operator-=(const Vec4 &other) -> Vec4 &;
  auto operator-=(Scalar scalar) -> Vec4 &;

  auto operator*(Scalar scalar) const -> Vec4;
  auto operator*(const Vec4 &other) const -> Vec4;
  auto operator*=(Scalar scalar) -> Vec4 &;
  auto operator*=(const Vec4 &other) -> Vec4 &;

  auto operator/(Scalar scalar) const -> Vec4;
  auto operator/(const Vec4 &other) const -> Vec4;
  auto operator/=(Scalar scalar) -> Vec4 &;
  auto operator/=(const Vec4 &other) -> Vec4 &;

  auto operator==(const Vec4 &other) const -> bool;
  auto operator!=(const Vec4 &other) const -> bool;

  auto operator[](uint32_t index) const -> Scalar;

  [[nodiscard]] auto Length() const -> Scalar;
  [[nodiscard]] auto Inverse() const -> Vec4;
  [[nodiscard]] auto Normalize() const -> Vec4;
  [[nodiscard]] auto Dot(const Vec4 &other) const -> Scalar;
  [[nodiscard]] auto Cross(const Vec4 &other) const -> Scalar;

  [[nodiscard]] auto Max(const Vec4 &other) const -> Vec4;
  [[nodiscard]] auto Max(Scalar scalar) const -> Vec4;
  [[nodiscard]] auto Min(const Vec4 &other) const -> Vec4;
  [[nodiscard]] auto Min(Scalar scalar) const -> Vec4;

  Vec4(Scalar x_val, Scalar y_val, Scalar z_val, Scalar w_val)
      : x(x_val), y(y_val), z(z_val), w(w_val) {}
  Vec4() = default;
  explicit Vec4(const Vec2 &vec2, Scalar z_val = 0.0F, Scalar w_val = 0.0F);
  explicit Vec4(const Vec3 &vec3, Scalar w_val = 0.0F);

  explicit Vec4(const Ivec2 &vec2, Scalar z_val = 0.0F, Scalar w_val = 0.0F);
  explicit Vec4(const Ivec3 &vec3, Scalar w_val = 0.0F);
  explicit Vec4(const Ivec4 &vec4);

  explicit Vec4(const Uvec2 &vec2, Scalar z_val = 0.0F, Scalar w_val = 0.0F);
  explicit Vec4(const Uvec3 &vec3, Scalar w_val = 0.0F);
  explicit Vec4(const Uvec4 &vec4);
};

struct Uvec2 {
  uint32_t x;
  uint32_t y;

  auto operator+(const Uvec2 &other) const -> Uvec2;
  auto operator+(uint32_t scalar) const -> Uvec2;
  auto operator+=(const Uvec2 &other) -> Uvec2 &;
  auto operator+=(uint32_t scalar) -> Uvec2 &;

  auto operator-(const Uvec2 &other) const -> Uvec2;
  auto operator-(uint32_t scalar) const -> Uvec2;
  auto operator-=(const Uvec2 &other) -> Uvec2 &;
  auto operator-=(uint32_t scalar) -> Uvec2 &;

  auto operator*(uint32_t scalar) const -> Uvec2;
  auto operator*(const Uvec2 &other) const -> Uvec2;
  auto operator*=(uint32_t scalar) -> Uvec2 &;
  auto operator*=(const Uvec2 &other) -> Uvec2 &;

  auto operator/(uint32_t scalar) const -> Uvec2;
  auto operator/(const Uvec2 &other) const -> Uvec2;
  auto operator/=(uint32_t scalar) -> Uvec2 &;
  auto operator/=(const Uvec2 &other) -> Uvec2 &;

  auto operator==(const Uvec2 &other) const -> bool;
  auto operator!=(const Uvec2 &other) const -> bool;

  auto operator[](uint32_t index) const -> uint32_t;

  Uvec2(uint32_t x_val, uint32_t y_val) : x(x_val), y(y_val) {}
  Uvec2() = default;
  explicit Uvec2(const Uvec3 &vec3);
  explicit Uvec2(const Uvec4 &vec4);

  explicit Uvec2(const Ivec2 &vec2);
  explicit Uvec2(const Ivec3 &vec3);
  explicit Uvec2(const Ivec4 &vec4);
};

struct Uvec3 {
  uint32_t x;
  uint32_t y;
  uint32_t z;

  auto operator+(const Uvec3 &other) const -> Uvec3;
  auto operator+(uint32_t scalar) const -> Uvec3;
  auto operator+=(const Uvec3 &other) -> Uvec3 &;
  auto operator+=(uint32_t scalar) -> Uvec3 &;

  auto operator-(const Uvec3 &other) const -> Uvec3;
  auto operator-(uint32_t scalar) const -> Uvec3;
  auto operator-=(const Uvec3 &other) -> Uvec3 &;
  auto operator-=(uint32_t scalar) -> Uvec3 &;

  auto operator*(uint32_t scalar) const -> Uvec3;
  auto operator*(const Uvec3 &other) const -> Uvec3;
  auto operator*=(uint32_t scalar) -> Uvec3 &;
  auto operator*=(const Uvec3 &other) -> Uvec3 &;

  auto operator/(uint32_t scalar) const -> Uvec3;
  auto operator/(const Uvec3 &other) const -> Uvec3;
  auto operator/=(uint32_t scalar) -> Uvec3 &;
  auto operator/=(const Uvec3 &other) -> Uvec3 &;

  auto operator==(const Uvec3 &other) const -> bool;
  auto operator!=(const Uvec3 &other) const -> bool;

  auto operator[](uint32_t index) const -> uint32_t;

  Uvec3(uint32_t x_val, uint32_t y_val, uint32_t z_val)
      : x(x_val), y(y_val), z(z_val) {}
  Uvec3() = default;
  explicit Uvec3(const Uvec2 &vec2, uint32_t z_val = 0);
  explicit Uvec3(const Uvec4 &vec4);

  explicit Uvec3(const Ivec2 &vec2, uint32_t z_val = 0);
  explicit Uvec3(const Ivec3 &vec3);
  explicit Uvec3(const Ivec4 &vec4);
};

struct Uvec4 {
  uint32_t x;
  uint32_t y;
  uint32_t z;
  uint32_t w;

  auto operator+(const Uvec4 &other) const -> Uvec4;
  auto operator+(uint32_t scalar) const -> Uvec4;
  auto operator+=(const Uvec4 &other) -> Uvec4 &;
  auto operator+=(uint32_t scalar) -> Uvec4 &;

  auto operator-(const Uvec4 &other) const -> Uvec4;
  auto operator-(uint32_t scalar) const -> Uvec4;
  auto operator-=(const Uvec4 &other) -> Uvec4 &;
  auto operator-=(uint32_t scalar) -> Uvec4 &;

  auto operator*(uint32_t scalar) const -> Uvec4;
  auto operator*(const Uvec4 &other) const -> Uvec4;
  auto operator*=(uint32_t scalar) -> Uvec4 &;
  auto operator*=(const Uvec4 &other) -> Uvec4 &;

  auto operator/(uint32_t scalar) const -> Uvec4;
  auto operator/(const Uvec4 &other) const -> Uvec4;
  auto operator/=(uint32_t scalar) -> Uvec4 &;
  auto operator/=(const Uvec4 &other) -> Uvec4 &;

  auto operator==(const Uvec4 &other) const -> bool;
  auto operator!=(const Uvec4 &other) const -> bool;

  auto operator[](uint32_t index) const -> uint32_t;

  Uvec4(uint32_t x_val, uint32_t y_val, uint32_t z_val, uint32_t w_val)
      : x(x_val), y(y_val), z(z_val), w(w_val) {}
  Uvec4() = default;
  explicit Uvec4(const Uvec2 &vec2, uint32_t z_val = 0, uint32_t w_val = 0);
  explicit Uvec4(const Uvec3 &vec3, uint32_t w_val = 0);

  explicit Uvec4(const Ivec2 &vec2, uint32_t z_val = 0, uint32_t w_val = 0);
  explicit Uvec4(const Ivec3 &vec3, uint32_t w_val = 0);
  explicit Uvec4(const Ivec4 &vec4);
};

struct Ivec2 {
  int32_t x;
  int32_t y;

  auto operator+(const Ivec2 &other) const -> Ivec2;
  auto operator+(int32_t scalar) const -> Ivec2;
  auto operator+=(const Ivec2 &other) -> Ivec2 &;
  auto operator+=(int32_t scalar) -> Ivec2 &;

  auto operator-(const Ivec2 &other) const -> Ivec2;
  auto operator-(int32_t scalar) const -> Ivec2;
  auto operator-=(const Ivec2 &other) -> Ivec2 &;
  auto operator-=(int32_t scalar) -> Ivec2 &;

  auto operator*(int32_t scalar) const -> Ivec2;
  auto operator*(const Ivec2 &other) const -> Ivec2;
  auto operator*=(int32_t scalar) -> Ivec2 &;
  auto operator*=(const Ivec2 &other) -> Ivec2 &;

  auto operator/(int32_t scalar) const -> Ivec2;
  auto operator/(const Ivec2 &other) const -> Ivec2;
  auto operator/=(int32_t scalar) -> Ivec2 &;
  auto operator/=(const Ivec2 &other) -> Ivec2 &;

  auto operator==(const Ivec2 &other) const -> bool;
  auto operator!=(const Ivec2 &other) const -> bool;

  auto operator[](uint32_t index) const -> int32_t;

  Ivec2(int32_t x_val, int32_t y_val) : x(x_val), y(y_val) {}
  Ivec2() = default;
  explicit Ivec2(const Ivec3 &vec3);
  explicit Ivec2(const Ivec4 &vec4);

  explicit Ivec2(const Uvec2 &vec2);
  explicit Ivec2(const Uvec3 &vec3);
  explicit Ivec2(const Uvec4 &vec4);
};

struct Ivec3 {
  int32_t x;
  int32_t y;
  int32_t z;

  auto operator+(const Ivec3 &other) const -> Ivec3;
  auto operator+(int32_t scalar) const -> Ivec3;
  auto operator+=(const Ivec3 &other) -> Ivec3 &;
  auto operator+=(int32_t scalar) -> Ivec3 &;

  auto operator-(const Ivec3 &other) const -> Ivec3;
  auto operator-(int32_t scalar) const -> Ivec3;
  auto operator-=(const Ivec3 &other) -> Ivec3 &;
  auto operator-=(int32_t scalar) -> Ivec3 &;

  auto operator*(int32_t scalar) const -> Ivec3;
  auto operator*(const Ivec3 &other) const -> Ivec3;
  auto operator*=(int32_t scalar) -> Ivec3 &;
  auto operator*=(const Ivec3 &other) -> Ivec3 &;

  auto operator/(int32_t scalar) const -> Ivec3;
  auto operator/(const Ivec3 &other) const -> Ivec3;
  auto operator/=(int32_t scalar) -> Ivec3 &;
  auto operator/=(const Ivec3 &other) -> Ivec3 &;

  auto operator==(const Ivec3 &other) const -> bool;
  auto operator!=(const Ivec3 &other) const -> bool;

  auto operator[](uint32_t index) const -> int32_t;

  Ivec3(int32_t x_val, int32_t y_val, int32_t z_val)
      : x(x_val), y(y_val), z(z_val) {}
  Ivec3() = default;
  explicit Ivec3(const Ivec2 &vec2, int32_t z_val = 0);
  explicit Ivec3(const Ivec4 &vec4);

  explicit Ivec3(const Uvec2 &vec2, int32_t z_val = 0);
  explicit Ivec3(const Uvec3 &vec3);
  explicit Ivec3(const Uvec4 &vec4);
};

struct Ivec4 {
  int32_t x;
  int32_t y;
  int32_t z;
  int32_t w;

  auto operator+(const Ivec4 &other) const -> Ivec4;
  auto operator+(int32_t scalar) const -> Ivec4;
  auto operator+=(const Ivec4 &other) -> Ivec4 &;
  auto operator+=(int32_t scalar) -> Ivec4 &;

  auto operator-(const Ivec4 &other) const -> Ivec4;
  auto operator-(int32_t scalar) const -> Ivec4;
  auto operator-=(const Ivec4 &other) -> Ivec4 &;
  auto operator-=(int32_t scalar) -> Ivec4 &;

  auto operator*(int32_t scalar) const -> Ivec4;
  auto operator*(const Ivec4 &other) const -> Ivec4;
  auto operator*=(int32_t scalar) -> Ivec4 &;
  auto operator*=(const Ivec4 &other) -> Ivec4 &;

  auto operator/(int32_t scalar) const -> Ivec4;
  auto operator/(const Ivec4 &other) const -> Ivec4;
  auto operator/=(int32_t scalar) -> Ivec4 &;
  auto operator/=(const Ivec4 &other) -> Ivec4 &;

  auto operator==(const Ivec4 &other) const -> bool;
  auto operator!=(const Ivec4 &other) const -> bool;

  auto operator[](uint32_t index) const -> int32_t;

  Ivec4(int32_t x_val, int32_t y_val, int32_t z_val, int32_t w_val)
      : x(x_val), y(y_val), z(z_val), w(w_val) {}
  Ivec4() = default;
  explicit Ivec4(const Ivec2 &vec2, int32_t z_val = 0, int32_t w_val = 0);
  explicit Ivec4(const Ivec3 &vec3, int32_t w_val = 0);

  explicit Ivec4(const Uvec2 &vec2, int32_t z_val = 0, int32_t w_val = 0);
  explicit Ivec4(const Uvec3 &vec3, int32_t w_val = 0);
  explicit Ivec4(const Uvec4 &vec4);
};

} // namespace Math