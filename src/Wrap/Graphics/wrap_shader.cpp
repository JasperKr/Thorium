
#include "Wrap/Graphics/wrap_shader.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/shader.hpp"
#include "Wrap/wrap.hpp"
#include <lauxlib.h>
#include <lua.h>
namespace Graphics {

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

auto Wrap_Send(lua_State *state) {
  auto *shader = LuaWrap::FromLuaObject<Shader::ShaderModule>(state, 1);
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
                               uniformName, Ref<Graphics::Buffer>(buffer));
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.ToString().c_str());
    }
  } else if (lua_isnumber(state, 3) != 0) {
    auto value = static_cast<float>(lua_tonumber(state, 3));
    auto result = shader->Send(*Graphics::GetCurrentGraphicsContext(),
                               uniformName, value);
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
  shader->Destroy(Graphics::GetCurrentGraphicsContext()->device);
  return 0;
}

} // namespace Graphics