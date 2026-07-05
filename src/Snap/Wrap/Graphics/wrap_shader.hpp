#pragma once

#include "Graphics/shader.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Graphics::Shader {

auto wrap_Send(lua_State *state) -> int;
auto wrap_HasUniform(lua_State *state) -> int;
auto wrap_GetUniforms(lua_State *state) -> int;
auto wrap_GetThreadgroupSize(lua_State *state) -> int;
auto wrap_GetWaveSize(lua_State *state) -> int;

auto wrap_NewShader(lua_State *state) -> int;
auto wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ShaderLib = {
    {"send", wrap_Send},
    {"hasUniform", wrap_HasUniform},
    {"getUniforms", wrap_GetUniforms},
    {"getThreadgroupSize", wrap_GetThreadgroupSize},
    {"getWaveSize", wrap_GetWaveSize},

};

extern "C" inline auto luaopen_shader(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Graphics::Shader::GetType(),
                           ShaderLib); // NOLINT

  return 1;
}
} // namespace Wrap::Graphics::Shader
