#include "wrap_math.hpp"
#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/matrix.hpp"
#include "lua.hpp"

namespace Wrap::Math {
using namespace ::Math::Conversions;

using EulerAngle = ::Math::EulerAngle;
using Quaternion = ::Math::Quaternion;
using Matrix = ::Math::Matrix4x4;
using Scalar = ::Math::Scalar;

inline auto checkscalar(lua_State *state, int index) -> Scalar {
  return static_cast<Scalar>(luaL_checknumber(state, index));
}

inline auto MatrixToLua(lua_State *state, const Matrix &mat) -> int {
  for (int i = 0; i < Matrix::Size; ++i) {
    lua_pushnumber(state, mat.At(i));
  }

  return Matrix::Size;
}

inline auto MatrixToLua(lua_State *state, const ::Math::Matrix3x3 &mat) -> int {
  for (int i = 0; i < ::Math::Matrix3x3::Size; ++i) {
    lua_pushnumber(state, mat.At(i));
  }

  return ::Math::Matrix3x3::Size;
}

inline auto LuaToMatrix4x4(lua_State *state, int index) -> Matrix {
  Matrix mat;
  for (int i = 0; i < Matrix::Size; ++i) {
    lua_rawgeti(state, index, i + 1);
    mat.At(i) = checkscalar(state, -1);
  }
  lua_pop(state, Matrix::Size);
  return mat;
}

inline auto LuaToMatrix3x3(lua_State *state, int index) -> ::Math::Matrix3x3 {
  ::Math::Matrix3x3 mat;
  for (int i = 0; i < ::Math::Matrix3x3::Size; ++i) {
    lua_rawgeti(state, index, i + 1);
    mat.At(i) = checkscalar(state, -1);
  }
  lua_pop(state, ::Math::Matrix3x3::Size);
  return mat;
}

inline auto QuaternionToLua(lua_State *state, const Quaternion &quat) -> int {
  lua_pushnumber(state, quat.x);
  lua_pushnumber(state, quat.y);
  lua_pushnumber(state, quat.z);
  lua_pushnumber(state, quat.w);

  return 4;
}

inline auto EulerToLua(lua_State *state, const EulerAngle &euler) -> int {
  lua_pushnumber(state, euler.yaw);
  lua_pushnumber(state, euler.pitch);
  lua_pushnumber(state, euler.roll);

  return 3;
}

auto wrap_EulerToQuaternion(lua_State *state) -> int {
  auto yaw = checkscalar(state, 1);
  auto pitch = checkscalar(state, 2);
  auto roll = checkscalar(state, 3);

  auto quat = ToQuaternion(EulerAngle{yaw, pitch, roll});

  lua_pushnumber(state, quat.x);
  lua_pushnumber(state, quat.y);
  lua_pushnumber(state, quat.z);
  lua_pushnumber(state, quat.w);

  return 4;
}

auto wrap_EulerToMatrix(lua_State *state) -> int {
  auto yaw = checkscalar(state, 1);
  auto pitch = checkscalar(state, 2);
  auto roll = checkscalar(state, 3);

  auto mat = ToMatrix(EulerAngle{yaw, pitch, roll});

  return MatrixToLua(state, mat);
}

auto wrap_QuaternionToEuler(lua_State *state) -> int {
  auto quat_x = checkscalar(state, 1);
  auto quat_y = checkscalar(state, 2);
  auto quat_z = checkscalar(state, 3);
  auto quat_w = checkscalar(state, 4);

  auto euler = ToEuler(Quaternion{quat_x, quat_y, quat_z, quat_w});

  return EulerToLua(state, euler);
}
auto wrap_QuaternionToMatrix(lua_State *state) -> int {
  auto quat_x = checkscalar(state, 1);
  auto quat_y = checkscalar(state, 2);
  auto quat_z = checkscalar(state, 3);
  auto quat_w = checkscalar(state, 4);

  auto mat = ToMatrix(Quaternion{quat_x, quat_y, quat_z, quat_w});

  return MatrixToLua(state, mat);
}

auto wrap_MatrixToEuler(lua_State *state) -> int {
  // Check that we have a table with 16 numbers
  if (lua_objlen(state, 1) != 16) { // NOLINT
    return luaL_error(state, "Expected a table with 16 numbers for matrix");
  }

  auto euler = ToEuler(LuaToMatrix4x4(state, 1));

  return EulerToLua(state, euler);
}
auto wrap_MatrixToQuaternion(lua_State *state) -> int {
  if (lua_objlen(state, 1) != 16) { // NOLINT
    return luaL_error(state, "Expected a table with 16 numbers for matrix");
  }

  auto quat = ToQuaternion(LuaToMatrix4x4(state, 1));

  return QuaternionToLua(state, quat);
}

auto wrap_TranslationMatrix(lua_State *state) -> int {
  auto mat = Matrix::TranslationMatrix(
      checkscalar(state, 1), checkscalar(state, 2), checkscalar(state, 3));

  return MatrixToLua(state, mat);
}

auto wrap_ScaleMatrix(lua_State *state) -> int {
  auto mat = Matrix::ScaleMatrix(checkscalar(state, 1), checkscalar(state, 2),
                                 checkscalar(state, 3));

  return MatrixToLua(state, mat);
}

auto wrap_TransformMatrix(lua_State *state) -> int {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  auto mat = Matrix::TransformationMatrix(
      checkscalar(state, 1), checkscalar(state, 2), checkscalar(state, 3),
      checkscalar(state, 4), checkscalar(state, 5), checkscalar(state, 6),
      checkscalar(state, 7), checkscalar(state, 8), checkscalar(state, 9),
      checkscalar(state, 10));
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  return MatrixToLua(state, mat);
}

auto wrap_Random(lua_State *state) -> int {
  if (lua_gettop(state) == 0) {
    auto result = ::Math::Random();
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 1) {
    auto max = static_cast<int>(luaL_checkinteger(state, 1));
    auto result = ::Math::Random(max);
    lua_pushinteger(state, result);
    return 1;
  }

  if (lua_gettop(state) == 2) {

    auto min = luaL_checkinteger(state, 1);
    auto max = luaL_checkinteger(state, 2);

    auto result = ::Math::Random(min, max);
    lua_pushinteger(state, result);
    return 1;
  }

  return luaL_error(state, "Invalid number of arguments to math.random");
}

auto wrap_Noise(lua_State *state) -> int {
  if (lua_gettop(state) == 1) {
    auto x_channel = checkscalar(state, 1);

    auto result = ::Math::Noise(x_channel, 0);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 2) {
    auto x_channel = checkscalar(state, 1);
    auto y_channel = checkscalar(state, 2);

    auto result = ::Math::Noise(x_channel, y_channel, 0, 0);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 3) {
    auto x_channel = checkscalar(state, 1);
    auto y_channel = checkscalar(state, 2);
    auto z_channel = checkscalar(state, 3);

    auto result = ::Math::Noise(x_channel, y_channel, z_channel, 0, 0, 0);
    lua_pushnumber(state, result);
    return 1;
  }

  return luaL_error(state, "Invalid number of arguments to math.noise");
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
auto wrap_NoiseWrapped(lua_State *state) -> int {
  if (lua_gettop(state) == 2) {
    auto x_channel = checkscalar(state, 1);
    auto x_wrap = static_cast<uint>(luaL_checkinteger(state, 2));

    auto result = ::Math::Noise(x_channel, x_wrap);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 4) {
    auto x_channel = checkscalar(state, 1);
    auto y_channel = checkscalar(state, 2);
    auto x_wrap = static_cast<uint>(luaL_checkinteger(state, 3));
    auto y_wrap = static_cast<uint>(luaL_checkinteger(state, 4));

    auto result = ::Math::Noise(x_channel, y_channel, x_wrap, y_wrap);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 6) {
    auto x_channel = checkscalar(state, 1);
    auto y_channel = checkscalar(state, 2);
    auto z_channel = checkscalar(state, 3);
    auto x_wrap = static_cast<uint>(luaL_checkinteger(state, 4));
    auto y_wrap = static_cast<uint>(luaL_checkinteger(state, 5));
    auto z_wrap = static_cast<uint>(luaL_checkinteger(state, 6));

    auto result =
        ::Math::Noise(x_channel, y_channel, z_channel, x_wrap, y_wrap, z_wrap);
    lua_pushnumber(state, result);
    return 1;
  }

  return luaL_error(state, "Invalid number of arguments to math.noise");
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

} // namespace Wrap::Math
