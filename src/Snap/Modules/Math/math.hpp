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

}; // namespace Conversions
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

auto Random(int Min, int Max) -> int;
auto Random(int Max) -> int;
auto Random() -> Scalar;

auto Noise(Scalar x_channel, uint x_wrap) -> Scalar;
auto Noise(Scalar x_channel, Scalar y_channel, uint x_wrap, uint y_wrap)
    -> Scalar;
auto Noise(Scalar x_channel, Scalar y_channel, Scalar z_channel, uint x_wrap,
           uint y_wrap, uint z_wrap) -> Scalar;

}; // namespace Math