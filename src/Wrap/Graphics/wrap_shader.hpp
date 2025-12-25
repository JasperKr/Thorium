#pragma once

#include "Graphics/shader.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Graphics::Shader {

auto wrap_Send(lua_State *state) -> int;
auto wrap_HasUniform(lua_State *state) -> int;
auto wrap_GetUniforms(lua_State *state) -> int;

auto wrap_NewShader(lua_State *state) -> int;
auto wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg ShaderLib[] = {
    {"send", wrap_Send},
    {"hasUniform", wrap_HasUniform},
    {"getUniforms", wrap_GetUniforms},
    {"release", wrap_Release},
    {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_shader(lua_State *state) -> int {
  PrintDebug("Registering Shader Lua type.");

  LuaWrap::RegisterLuaType(state, Shader::ShaderModule::GetType(),
                           ShaderLib); // NOLINT

  return 1;
}
} // namespace Graphics::Shader
