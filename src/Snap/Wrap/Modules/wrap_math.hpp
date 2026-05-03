#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Math {

auto wrap_EulerToQuaternion(lua_State *state) -> int;
auto wrap_EulerToMatrix(lua_State *state) -> int;

auto wrap_QuaternionToEuler(lua_State *state) -> int;
auto wrap_QuaternionToMatrix(lua_State *state) -> int;

auto wrap_MatrixToEuler(lua_State *state) -> int;
auto wrap_MatrixToQuaternion(lua_State *state) -> int;

auto wrap_TranslationMatrix(lua_State *state) -> int;
auto wrap_ScaleMatrix(lua_State *state) -> int;
auto wrap_TransformMatrix(lua_State *state) -> int;

auto wrap_Random(lua_State *state) -> int;
auto wrap_Noise(lua_State *state) -> int;
auto wrap_NoiseWrapped(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> MathLib = {
    {"eulerToQuaternion", wrap_EulerToQuaternion},
    {"eulerToMatrix", wrap_EulerToMatrix},
    {"quaternionToEuler", wrap_QuaternionToEuler},
    {"quaternionToMatrix", wrap_QuaternionToMatrix},
    {"matrixToEuler", wrap_MatrixToEuler},
    {"matrixToQuaternion", wrap_MatrixToQuaternion},
    {"translationMatrix", wrap_TranslationMatrix},
    {"scaleMatrix", wrap_ScaleMatrix},
    {"transformMatrix", wrap_TransformMatrix},
    {"random", wrap_Random},
    {"noise", wrap_Noise},
    {"noiseWrapped", wrap_NoiseWrapped},

};

const static std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_engine_math(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "math",
      .Functions = MathLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,

  };

  RegisterLuaModule(state, module);
  return 1;
}
} // namespace Wrap::Math