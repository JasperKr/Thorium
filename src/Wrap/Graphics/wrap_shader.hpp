#pragma once

#include "Graphics/shader.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
#include <lauxlib.h>
#include <lua.h>
namespace Graphics {

auto Wrap_Send(lua_State *state) -> int;
auto Wrap_HasUniform(lua_State *state) -> int;
auto Wrap_GetUniforms(lua_State *state) -> int;

auto Wrap_NewShader(lua_State *state) -> int;
auto Wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg ShaderLib[] = {
    {"send", Wrap_Send},
    {"hasUniform", Wrap_HasUniform},
    {"getUniforms", Wrap_GetUniforms},
    {"release", Wrap_Release},
    {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_shader(lua_State *state) -> int {
  PrintDebug("Registering Shader Lua type.");

  LuaWrap::RegisterLuaType(state, Shader::ShaderModule::GetType(),
                           ShaderLib); // NOLINT

  return 1;
}

} // namespace Graphics