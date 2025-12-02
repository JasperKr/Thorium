#include "Graphics/texture.hpp"
#include "Wrap/wrap.hpp"
#include "vulkan/vulkan_core.h"
#include <lua.h>
namespace Graphics::Texture {
auto inline StringToVkFilter(const char *filterStr) -> VkFilter {
  if (strcmp(filterStr, "nearest") == 0) {
    return VK_FILTER_NEAREST;
  }
  if (strcmp(filterStr, "linear") == 0) {
    return VK_FILTER_LINEAR;
  }
  return VK_FILTER_LINEAR; // Default
}

auto Wrap_SetFilter(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  const char *minFilterStr = luaL_checkstring(state, 2);
  const char *magFilterStr = luaL_checkstring(state, 3);
  const char *mipFilterStr = luaL_checkstring(state, 4);

  VkFilter minFilter = StringToVkFilter(minFilterStr);
  VkFilter magFilter = StringToVkFilter(magFilterStr);
  VkFilter mipFilter_ = StringToVkFilter(mipFilterStr);
  auto mipFilter = (mipFilter_ == VK_FILTER_NEAREST)
                       ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                       : VK_SAMPLER_MIPMAP_MODE_LINEAR;

  texture->SetFilter(minFilter, magFilter, mipFilter);

  return 0;
}

auto Wrap_GetFilter(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  VkFilter minFilter = VK_FILTER_LINEAR;
  VkFilter magFilter = VK_FILTER_LINEAR;
  VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  std::tie(minFilter, magFilter, mipFilter) = texture->GetFilter();

  const char *minFilterStr =
      (minFilter == VK_FILTER_NEAREST) ? "nearest" : "linear";
  const char *magFilterStr =
      (magFilter == VK_FILTER_NEAREST) ? "nearest" : "linear";
  const char *mipFilterStr =
      (mipFilter == VK_SAMPLER_MIPMAP_MODE_NEAREST) ? "nearest" : "linear";

  lua_pushstring(state, minFilterStr);
  lua_pushstring(state, magFilterStr);
  lua_pushstring(state, mipFilterStr);

  return 3;
}

auto Wrap_SetAnisotropy(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  auto anisotropy = static_cast<float>(luaL_checknumber(state, 2));

  texture->SetAnisotropy(anisotropy);

  return 0;
}

auto Wrap_GetAnisotropy(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  float anisotropy = texture->GetAnisotropy();

  lua_pushnumber(state, static_cast<lua_Number>(anisotropy));
  return 1;
}

auto inline StringToAddressMode(const char *addressModeStr)
    -> VkSamplerAddressMode {
  if (strcmp(addressModeStr, "repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
  if (strcmp(addressModeStr, "mirrored_repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  if (strcmp(addressModeStr, "clamp_to_edge") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  return VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default
}

auto Wrap_SetWrapmode(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  const char *addressModeUStr = luaL_checkstring(state, 2);
  const char *addressModeVStr = luaL_checkstring(state, 3);
  const char *addressModeWStr = luaL_checkstring(state, 4);

  VkSamplerAddressMode addressModeU = StringToAddressMode(addressModeUStr);
  VkSamplerAddressMode addressModeV = StringToAddressMode(addressModeVStr);
  VkSamplerAddressMode addressModeW = StringToAddressMode(addressModeWStr);

  texture->SetWrapmode(addressModeU, addressModeV, addressModeW);

  return 0;
}

auto inline AddressModeToString(VkSamplerAddressMode addressMode) -> const
    char * {
  switch (addressMode) {
  case VK_SAMPLER_ADDRESS_MODE_REPEAT:
    return "repeat";
  case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
    return "mirrored_repeat";
  case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
    return "clamp_to_edge";
  default:
    return "repeat";
  }
}

auto Wrap_GetWrapmode(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  std::tie(addressModeU, addressModeV, addressModeW) = texture->GetWrapmode();

  const char *addressModeUStr = AddressModeToString(addressModeU);
  const char *addressModeVStr = AddressModeToString(addressModeV);
  const char *addressModeWStr = AddressModeToString(addressModeW);

  lua_pushstring(state, addressModeUStr);
  lua_pushstring(state, addressModeVStr);
  lua_pushstring(state, addressModeWStr);

  return 3;
}

auto Wrap_SetLodBias(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  auto mipLodBias = static_cast<float>(luaL_checknumber(state, 2));

  texture->SetLodBias(mipLodBias);

  return 0;
}

auto Wrap_GetLodBias(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  float mipLodBias = texture->GetLodBias();

  lua_pushnumber(state, static_cast<lua_Number>(mipLodBias));
  return 1;
}

auto Wrap_SetLodRange(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  auto minLod = static_cast<float>(luaL_checknumber(state, 2));
  auto maxLod = static_cast<float>(luaL_checknumber(state, 3));

  texture->SetLodRange(minLod, maxLod);

  return 0;
}

auto Wrap_GetLodRange(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  float minLod = 0.0F;
  float maxLod = 0.0F;
  std::tie(minLod, maxLod) = texture->GetLodRange();

  lua_pushnumber(state, static_cast<lua_Number>(minLod));
  lua_pushnumber(state, static_cast<lua_Number>(maxLod));
  return 2;
}

auto Wrap_SetDepthCompare(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  auto enable = lua_toboolean(state, 2) == 1;
  auto compareOp = static_cast<VkCompareOp>(luaL_checkinteger(state, 3));

  texture->SetDepthCompare(enable, compareOp);

  return 0;
}

auto Wrap_GetDepthCompare(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  bool enable = false;
  VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
  std::tie(enable, compareOp) = texture->GetDepthCompare();

  lua_pushboolean(state, enable ? 1 : 0);
  lua_pushinteger(state, static_cast<lua_Integer>(compareOp));
  return 2;
}

auto Wrap_GetWidth(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  uint32_t width = texture->GetWidth();

  lua_pushinteger(state, static_cast<lua_Integer>(width));
  return 1;
}

auto Wrap_GetHeight(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  uint32_t height = texture->GetHeight();

  lua_pushinteger(state, static_cast<lua_Integer>(height));
  return 1;
}

auto Wrap_GetDepth(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  uint32_t depth = texture->GetDepth();

  lua_pushinteger(state, static_cast<lua_Integer>(depth));
  return 1;
}

auto Wrap_GetDimensions(lua_State *state) -> int {
  auto *texture = LuaWrap::FromLuaObject<Texture>(state, 1);
  VkExtent2D dimensions = texture->GetDimensions();

  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.width));
  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.height));
  return 2;
}

} // namespace Graphics::Texture