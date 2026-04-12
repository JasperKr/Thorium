#include "material.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/wrap.hpp"
#include "entity.hpp"
#include "lua.hpp"
#include <cstdint>
#include <imgui.h>

namespace Engine::Renderer {

const static LuaWrap::LuaEnum<AlphaMode>
    LuaAlphaModeEnum("AlphaMode", {
                                      {"opaque", AlphaMode::Opaque},
                                      {"mask", AlphaMode::Mask},
                                      {"blend", AlphaMode::Blend},
                                  });

const static LuaWrap::LuaEnum<VkCullModeFlags>
    LuaCullModeEnum("CullMode", {
                                    {"none", VK_CULL_MODE_NONE},
                                    {"front", VK_CULL_MODE_FRONT_BIT},
                                    {"back", VK_CULL_MODE_BACK_BIT},
                                    {"all", VK_CULL_MODE_FRONT_AND_BACK},
                                });

auto Material::wrap_setTextureUVIndex(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto textureType = luaL_checkinteger(state, 2);
  if (textureType < 0 ||
      textureType >= static_cast<int>(material->textureUVIndices.size())) {
    return luaL_error(state, "Invalid texture type index");
  }

  auto uvIndex = luaL_checkinteger(state, 3);
  if (uvIndex < 0 || uvIndex > UINT8_MAX) {
    return luaL_error(state, "UV index must be between 0 and UINT8_MAX");
  }

  material->textureUVIndices.at(textureType) = static_cast<uint8_t>(uvIndex);
  return 0;
}

auto Material::wrap_getTextureUVIndex(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }
  auto textureType = luaL_checkinteger(state, 2);
  if (textureType < 0 ||
      textureType >= static_cast<int>(material->textureUVIndices.size())) {
    return luaL_error(state, "Invalid texture type index");
  }

  lua_pushinteger(state, material->textureUVIndices.at(textureType));

  return 1;
}

auto Material::wrap_setCullMode(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaCullModeEnum.FromLua(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.error().message.c_str());
  }

  material->cullMode = result.value();
  return 0;
}

auto Material::wrap_getCullMode(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaCullModeEnum.ToLua(state, material->cullMode);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.message.c_str());
  }

  return 1;
}

auto Material::wrap_setAlphaMode(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaAlphaModeEnum.FromLua(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.error().message.c_str());
  }

  material->alphaMode = result.value();
  return 0;
}

auto Material::wrap_getAlphaMode(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaAlphaModeEnum.ToLua(state, material->alphaMode);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.message.c_str());
  }

  return 1;
}

} // namespace Engine::Renderer