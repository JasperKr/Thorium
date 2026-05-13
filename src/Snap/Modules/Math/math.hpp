#pragma once

#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"

namespace Math {

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
namespace Conversions {

auto ToEuler(Quaternion quaternion) -> EulerAngle;
auto ToEuler(Matrix4x4 matrix) -> EulerAngle;

auto ToQuaternion(EulerAngle euler) -> Quaternion;
auto ToQuaternion(Matrix4x4 matrix) -> Quaternion;

auto ToMatrix(EulerAngle euler) -> Matrix4x4;
auto ToMatrix(Quaternion quat) -> Matrix4x4;

auto ToMatrix3x3(EulerAngle euler) -> Matrix3x3;
auto ToMatrix3x3(Quaternion quat) -> Matrix3x3;

}; // namespace Conversions
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

auto Random(int Min, int Max) -> int;
auto Random(Scalar Min, Scalar Max) -> Scalar;
auto Random(long Min, long Max) -> long;
auto Random(int Max) -> int;
auto Random() -> Scalar;

auto Noise(Scalar x_channel, uint x_wrap) -> Scalar;
auto Noise(Scalar x_channel, Scalar y_channel, uint x_wrap, uint y_wrap)
    -> Scalar;
auto Noise(Scalar x_channel, Scalar y_channel, Scalar z_channel, uint x_wrap,
           uint y_wrap, uint z_wrap) -> Scalar;

auto Abs(Scalar value) -> Scalar;
auto Abs(Vec2 vec) -> Vec2;
auto Abs(Vec3 vec) -> Vec3;
auto Abs(Vec4 vec) -> Vec4;
auto Abs(Ivec2 vec) -> Ivec2;
auto Abs(Ivec3 vec) -> Ivec3;
auto Abs(Ivec4 vec) -> Ivec4;
auto Abs(Matrix3x3 mat) -> Matrix3x3;
auto Abs(Matrix4x4 mat) -> Matrix4x4;

auto Floor(Scalar value) -> Scalar;
auto Floor(Vec2 vec) -> Vec2;
auto Floor(Vec3 vec) -> Vec3;
auto Floor(Vec4 vec) -> Vec4;
auto Floor(Uvec2 vec) -> Uvec2;
auto Floor(Uvec3 vec) -> Uvec3;
auto Floor(Uvec4 vec) -> Uvec4;
auto Floor(Ivec2 vec) -> Ivec2;
auto Floor(Ivec3 vec) -> Ivec3;
auto Floor(Ivec4 vec) -> Ivec4;

auto Ceil(Scalar value) -> Scalar;
auto Ceil(Vec2 vec) -> Vec2;
auto Ceil(Vec3 vec) -> Vec3;
auto Ceil(Vec4 vec) -> Vec4;
auto Ceil(Uvec2 vec) -> Uvec2;
auto Ceil(Uvec3 vec) -> Uvec3;
auto Ceil(Uvec4 vec) -> Uvec4;
auto Ceil(Ivec2 vec) -> Ivec2;
auto Ceil(Ivec3 vec) -> Ivec3;
auto Ceil(Ivec4 vec) -> Ivec4;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
constexpr auto RadToDeg(Scalar radians) -> Scalar {
  return radians * (180.0F / std::numbers::pi_v<Scalar>);
}

constexpr auto DegToRad(Scalar degrees) -> Scalar {
  return degrees * (std::numbers::pi_v<Scalar> / 180.0F);
}

constexpr auto RadToTurns(Scalar radians) -> Scalar {
  return radians / (2.0F * std::numbers::pi_v<Scalar>);
}

constexpr auto DegToTurns(Scalar degrees) -> Scalar { return degrees / 360.0F; }

constexpr auto TurnsToRad(Scalar turns) -> Scalar {
  return turns * (2.0F * std::numbers::pi_v<Scalar>);
}

constexpr auto TurnsToDeg(Scalar turns) -> Scalar { return turns * 360.0F; }
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

}; // namespace Math