
#include "Wrap/Graphics/wrap_shader.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/shader.hpp"
#include "Wrap/wrap.hpp"
#include <lauxlib.h>
#include <lua.h>
namespace Graphics::Shader {

// TODO: Add externs input support
// Modulename, {name=value, ...}
auto Wrap_NewShader(lua_State *state) -> int {
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  const auto *type = Graphics::Shader::ShaderModule::GetType();
  int args = lua_gettop(state);

  const char *name = luaL_checkstring(state, 1); // module name

  std::string shaderDebugName = "Shader_" + std::string(name);

  if (args == 2 && lua_istable(state, 2)) {
    lua_getfield(state, 2, "name");
    const char *customName = lua_tostring(state, -1);
    lua_pop(state, 1);

    if (customName != nullptr) {
      shaderDebugName = std::string(customName);
    }
  }

  auto result = Graphics::Shader::ShaderModule::Create(*ctx, std::string(name),
                                                       shaderDebugName);

  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.error().ToString().c_str());
  }

  LuaWrap::PushLuaType(state, type, result.value().get());
  result.value()->release(); // Retained by lua now

  return 1;
}

auto Wrap_Send(lua_State *state) -> int {
  auto *shader = LuaWrap::FromLuaObject<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  const char *uniformName = luaL_checkstring(state, 2);

  if (LuaWrap::LuaIsType<Graphics::Texture::Texture>(state, 3)) {
    auto *texture =
        LuaWrap::FromLuaObject<Graphics::Texture::Texture>(state, 3);
    auto result = shader->Send(*Graphics::GetCurrentGraphicsContext(),
                               uniformName, texture);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.ToString().c_str());
    }
  } else if (LuaWrap::LuaIsType<Graphics::Buffer>(state, 3)) {
    auto *buffer = LuaWrap::FromLuaObject<Graphics::Buffer>(state, 3);
    auto result = shader->Send(*Graphics::GetCurrentGraphicsContext(),
                               uniformName, buffer);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.ToString().c_str());
    }
  } else if (lua_isnumber(state, 3) != 0) {
    auto varargsCount = lua_gettop(state) - 2;
    std::vector<float> data;

    data.resize(sizeof(float) * static_cast<size_t>(varargsCount));
    for (int i = 0; i < varargsCount; ++i) {
      data.emplace_back(lua_tonumber(state, 3 + i));
    }

    auto span = std::span<uint8_t>( // NOLINTNEXTLINE reinterpret cast
        reinterpret_cast<uint8_t *>(data.data()),
        sizeof(float) * static_cast<size_t>(varargsCount));

    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), uniformName, span);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.ToString().c_str());
    }
  } else {
    return luaL_error(state, "Unsupported uniform type.");
  }

  return 0;
}

auto Wrap_Release(lua_State *state) -> int {
  auto *shader = LuaWrap::FromLuaObject<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  shader->Destroy(Graphics::GetCurrentGraphicsContext()->device);
  return 0;
}

auto Wrap_HasUniform(lua_State *state) -> int {
  auto *shader = LuaWrap::FromLuaObject<Shader::ShaderModule>(state, 1);

  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  const char *uniformName = luaL_checkstring(state, 2);

  lua_pushboolean(state, 0);
  return 1;
}
auto Wrap_GetUniforms(lua_State *state) -> int {
  auto *shader = LuaWrap::FromLuaObject<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  return 0;
}

} // namespace Graphics::Shader