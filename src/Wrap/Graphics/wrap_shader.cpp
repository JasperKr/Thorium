
#include "Wrap/Graphics/wrap_shader.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/shader.hpp"
#include "Wrap/wrap.hpp"
#include <lauxlib.h>
#include <lua.h>
namespace Graphics::Shader {

// TODO: Add externs input support
// Modulename, {name=value, ...}
auto wrap_NewShader(lua_State *state) -> int {
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
    return luaL_error(state, "%s", result.error().message.c_str());
  }

  LuaWrap::PushLuaType(state, type, result.value().get());
  result.value()->release(); // Retained by lua now

  return 1;
}

// linked list of strings as key and a count of valid entries
inline auto LoadKey(lua_State *state, int index)
    -> std::pair<ResourceKey, int32_t> {
  auto count = lua_gettop(state);
  ResourceKey root;
  auto iterator = root.before_begin();

  for (int i = index; i <= count; ++i) {
    if (lua_isstring(state, i) != 0) {
      iterator = root.insert_after(iterator, luaL_checkstring(state, i));
    } else {
      return std::make_pair(root, i - index);
    }
  }

  return std::make_pair(root, count);
}

auto Wrap_Send(lua_State *state) -> int {
  auto *shader = LuaWrap::FromLuaObject<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  auto [key, keyCount] = LoadKey(state, 2);
  if (keyCount == 0) {
    return luaL_error(state, "Invalid uniform name.");
  }

  if (LuaWrap::LuaIsType<Graphics::Texture::Texture>(state, 3)) {
    auto *texture =
        LuaWrap::FromLuaObject<Graphics::Texture::Texture>(state, 3);
    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, texture);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
    }
  } else if (LuaWrap::LuaIsType<Graphics::Buffer>(state, 3)) {
    auto *buffer = LuaWrap::FromLuaObject<Graphics::Buffer>(state, 3);
    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, buffer);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
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
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, span);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
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