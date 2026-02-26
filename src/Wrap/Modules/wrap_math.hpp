#pragma once

#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Math {

auto wrap_EulerToQuaternion(lua_State *state) -> int;
auto wrap_EulerToMatrix(lua_State *state) -> int;

auto wrap_QuaternionToEuler(lua_State *state) -> int;
auto wrap_QuaternionToMatrix(lua_State *state) -> int;

auto wrap_MatrixToEuler(lua_State *state) -> int;
auto wrap_MatrixToQuaternion(lua_State *state) -> int;

auto wrap_Random(lua_State *state) -> int;
auto wrap_Noise(lua_State *state) -> int;
auto wrap_NoiseWrapped(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg MathLib[] = {
    {"eulerToQuaternion", wrap_EulerToQuaternion},
    {"eulerToMatrix", wrap_EulerToMatrix},
    {"quaternionToEuler", wrap_QuaternionToEuler},
    {"quaternionToMatrix", wrap_QuaternionToMatrix},
    {"matrixToEuler", wrap_MatrixToEuler},
    {"matrixToQuaternion", wrap_MatrixToQuaternion},
    {"random", wrap_Random},
    {"noise", wrap_Noise},
    {"noiseWrapped", wrap_NoiseWrapped},
    {nullptr, nullptr},
};

// nullptr-terminated
const static lua_CFunction *const childrenInitFunctions = {nullptr};

extern "C" inline auto luaopen_engine_math(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "math",
      .Functions = MathLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}
} // namespace Wrap::Math