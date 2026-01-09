
#include "Wrap/Graphics/wrap_shader.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/shader.hpp"
#include "Modules/bytedata.hpp"
#include "Wrap/Graphics/wrap_reflection.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_utils.hpp"
#include <bit>
#include <cstdint>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
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

  LuaWrap::PushObject(state, type, result.value().get());
  // result.value()->release(); // Retained by lua now

  return 1;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto wrap_Send(lua_State *state) -> int {
  auto *shader = LuaWrap::ObjectFromLua<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  auto [key, keyCount] = ResourceKeyFromLua(state, 2);
  if (keyCount == 0) {
    return luaL_error(state, "Invalid uniform name.");
  }

  // Base index of 1; + 1 for shader object; + keyCount for key parts
  auto valueOffset = 2 + keyCount;

  if (LuaWrap::IsType<Graphics::Texture::Texture>(state, valueOffset)) {
    auto *texture =
        LuaWrap::ObjectFromLua<Graphics::Texture::Texture>(state, valueOffset);
    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, texture);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
    }
  } else if (LuaWrap::IsType<Graphics::StructuredBuffer::StructuredBuffer>(
                 state, valueOffset)) {
    auto *buffer =
        LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
            state, valueOffset);
    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, buffer);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
    }
  } else if (lua_type(state, valueOffset) == LUA_TNUMBER) {
    auto varargsCount = lua_gettop(state) - valueOffset + 1;
    std::vector<uint8_t> data{};

    data.reserve(sizeof(uint32_t) * static_cast<size_t>(varargsCount));

    auto uniformResult = shader->GetUniform(key);
    if (Error::IsError(uniformResult)) {
      return luaL_error(state, "%s", uniformResult.error().message.c_str());
    }
    const auto &uniformInfo = uniformResult.value();
    if (!(uniformInfo.IsScalar() || uniformInfo.IsVector() ||
          uniformInfo.IsMatrix())) {
      return luaL_error(
          state, "Unable to send uniform `%s`: expected %s, got number",
          ResourceKeyToString(key).c_str(), uniformInfo.GetTypename().c_str());
    }

    ScalarType scalarType{};

    if (uniformInfo.Is<ScalarInfo>()) {
      scalarType = uniformInfo.GetInfo<ScalarInfo>().type;
    } else if (uniformInfo.Is<VectorInfo>()) {
      scalarType = uniformInfo.GetInfo<VectorInfo>().scalarType;
    } else if (uniformInfo.Is<MatrixInfo>()) {
      scalarType = ScalarType::Float;
    }

    if (scalarType == ScalarType::Unknown) {
      return luaL_error(state,
                        "Unable to send uniform `%s`: unknown scalar type",
                        ResourceKeyToString(key).c_str());
    }

    for (int i = 0; i < varargsCount; ++i) {
      auto result =
          Wrap::Utils::SetData(lua_tonumber(state, valueOffset + i),
                               data.data() + (i * sizeof(uint32_t)), // NOLINT
                               scalarType);

      if (Error::IsError(result)) {
        return luaL_error(state, "%s", result.message.c_str());
      }
    }

    auto span = std::span<uint8_t>( // NOLINTNEXTLINE reinterpret cast
        reinterpret_cast<uint8_t *>(data.data()),
        sizeof(uint32_t) * static_cast<size_t>(varargsCount));

    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, span);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
    }
  } else if (lua_type(state, valueOffset) == LUA_TTABLE) {
    std::vector<uint32_t> data;
    uint64_t tableLength = lua_objlen(state, valueOffset);
    data.reserve(sizeof(uint32_t) * static_cast<size_t>(tableLength));

    for (uint64_t i = 0; i < tableLength; ++i) {
      lua_rawgeti(state, valueOffset, static_cast<int>(i + 1));
      auto value = static_cast<float>(lua_tonumber(state, -1));

      data.emplace_back(std::bit_cast<uint32_t>(value));
      lua_pop(state, 1);
    }

    auto span = std::span<uint8_t>( // NOLINTNEXTLINE reinterpret cast
        reinterpret_cast<uint8_t *>(data.data()),
        sizeof(uint32_t) * static_cast<size_t>(tableLength));

    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, span);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
    }
  } else if (LuaWrap::IsType<Data::ByteData>(state, valueOffset)) {
    auto *byteData = LuaWrap::ObjectFromLua<Data::ByteData>(state, valueOffset);
    auto span = byteData->GetDataSpan();

    auto result =
        shader->Send(*Graphics::GetCurrentGraphicsContext(), key, span);
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.message.c_str());
    }
  } else {
    const auto resourceInfo = shader->GetUniform(key);
    if (Error::IsError(resourceInfo)) {
      return luaL_error(state, "%s", resourceInfo.error().message.c_str());
    }

    return luaL_error(state, "Unable to send uniform `%s`: expected %s, got %s",
                      ResourceKeyToString(key).c_str(),
                      resourceInfo->GetTypename().c_str(),
                      luaL_typename(state, valueOffset));
  }

  return 0;
}

auto wrap_Release(lua_State *state) -> int {
  auto *shader = LuaWrap::ObjectFromLua<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  shader->Destroy(Graphics::GetCurrentGraphicsContext()->device);
  return 0;
}

auto wrap_HasUniform(lua_State *state) -> int {
  auto *shader = LuaWrap::ObjectFromLua<Shader::ShaderModule>(state, 1);

  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  const char *uniformName = luaL_checkstring(state, 2);

  lua_pushboolean(state, 0);
  return 1;
}
auto wrap_GetUniforms(lua_State *state) -> int {
  auto *shader = LuaWrap::ObjectFromLua<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  return 0;
}
auto wrap_GetThreadgroupSize(lua_State *state) -> int {
  auto *shader = LuaWrap::ObjectFromLua<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  auto threadgroupSizeResult = shader->GetThreadgroupSize();
  if (Error::IsError(threadgroupSizeResult)) {
    return luaL_error(state, "%s",
                      threadgroupSizeResult.error().message.c_str());
  }

  auto threadgroupSize = threadgroupSizeResult.value();

  lua_pushinteger(state, static_cast<lua_Integer>(threadgroupSize.x));
  lua_pushinteger(state, static_cast<lua_Integer>(threadgroupSize.y));
  lua_pushinteger(state, static_cast<lua_Integer>(threadgroupSize.z));
  return 3;
}

auto wrap_GetWaveSize(lua_State *state) -> int {
  auto *shader = LuaWrap::ObjectFromLua<Shader::ShaderModule>(state, 1);
  if (shader == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  auto waveSize = shader->GetWaveSize();

  lua_pushinteger(state, static_cast<lua_Integer>(waveSize));
  return 1;
}

} // namespace Graphics::Shader