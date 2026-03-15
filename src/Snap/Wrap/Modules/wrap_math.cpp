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

  // Row-major order
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      lua_pushnumber(state, mat.At(row, column));
    }
  }

  return 16; // NOLINT
}

auto wrap_QuaternionToEuler(lua_State *state) -> int {
  auto quat_x = checkscalar(state, 1);
  auto quat_y = checkscalar(state, 2);
  auto quat_z = checkscalar(state, 3);
  auto quat_w = checkscalar(state, 4);

  auto euler = ToEuler(Quaternion{quat_x, quat_y, quat_z, quat_w});

  lua_pushnumber(state, euler.yaw);
  lua_pushnumber(state, euler.pitch);
  lua_pushnumber(state, euler.roll);

  return 3;
}
auto wrap_QuaternionToMatrix(lua_State *state) -> int {
  auto quat_x = checkscalar(state, 1);
  auto quat_y = checkscalar(state, 2);
  auto quat_z = checkscalar(state, 3);
  auto quat_w = checkscalar(state, 4);

  auto mat = ToMatrix(Quaternion{quat_x, quat_y, quat_z, quat_w});

  // Row-major order
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      lua_pushnumber(state, mat.At(row, column));
    }
  }

  return 16; // NOLINT
}

auto wrap_MatrixToEuler(lua_State *state) -> int {
  // Check that we have a table with 16 numbers
  if (lua_objlen(state, 1) != 16) { // NOLINT
    return luaL_error(state, "Expected a table with 16 numbers for matrix");
  }

  Matrix mat;
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      lua_rawgeti(state, 1,
                  (column * 4) + row + 1); // Get the value at the correct index
      mat.At(row, column) = checkscalar(
          state, -1); // Check that it's a number and assign it to the matrix
    }
  }
  lua_pop(state, 16); // Pop the values from the stack

  auto euler = ToEuler(mat);

  lua_pushnumber(state, euler.yaw);
  lua_pushnumber(state, euler.pitch);
  lua_pushnumber(state, euler.roll);

  return 3;
}
auto wrap_MatrixToQuaternion(lua_State *state) -> int {
  if (lua_objlen(state, 1) != 16) { // NOLINT
    return luaL_error(state, "Expected a table with 16 numbers for matrix");
  }

  Matrix mat;
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      lua_rawgeti(state, 1,
                  (column * 4) + row + 1); // Get the value at the correct index
      mat.At(row, column) = checkscalar(
          state, -1); // Check that it's a number and assign it to the matrix
    }
  }
  lua_pop(state, 16); // Pop the values from the stack

  auto quat = ToQuaternion(mat);

  lua_pushnumber(state, quat.x);
  lua_pushnumber(state, quat.y);
  lua_pushnumber(state, quat.z);
  lua_pushnumber(state, quat.w);

  return 4;
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

    auto min = static_cast<int>(luaL_checkinteger(state, 1));
    auto max = static_cast<int>(luaL_checkinteger(state, 2));

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
