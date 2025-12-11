#pragma once

#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
#include <lauxlib.h>
#include <lua.h>
namespace Graphics::Texture {
auto Wrap_SetFilter(lua_State *state) -> int;
auto Wrap_GetFilter(lua_State *state) -> int;
auto Wrap_SetAnisotropy(lua_State *state) -> int;
auto Wrap_GetAnisotropy(lua_State *state) -> int;
auto Wrap_SetWrapmode(lua_State *state) -> int;
auto Wrap_GetWrapmode(lua_State *state) -> int;
auto Wrap_SetLodBias(lua_State *state) -> int;
auto Wrap_GetLodBias(lua_State *state) -> int;
auto Wrap_SetLodRange(lua_State *state) -> int;
auto Wrap_GetLodRange(lua_State *state) -> int;
auto Wrap_SetDepthCompare(lua_State *state) -> int;
auto Wrap_GetDepthCompare(lua_State *state) -> int;
auto Wrap_GetWidth(lua_State *state) -> int;
auto Wrap_GetHeight(lua_State *state) -> int;
auto Wrap_GetDepth(lua_State *state) -> int;
auto Wrap_GetDimensions(lua_State *state) -> int;
auto Wrap_GetMipmapCount(lua_State *state) -> int;
auto Wrap_GetFormat(lua_State *state) -> int;

auto wrap_NewTexture(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg TextureLib[] = {
    {"setFilter", Wrap_SetFilter},
    {"getFilter", Wrap_GetFilter},
    {"setAnisotropy", Wrap_SetAnisotropy},
    {"getAnisotropy", Wrap_GetAnisotropy},
    {"setWrap", Wrap_SetWrapmode},
    {"getWrap", Wrap_GetWrapmode},
    {"setLodBias", Wrap_SetLodBias},
    {"getLodBias", Wrap_GetLodBias},
    {"setLodRange", Wrap_SetLodRange},
    {"getLodRange", Wrap_GetLodRange},
    {"setDepthCompare", Wrap_SetDepthCompare},
    {"getDepthCompare", Wrap_GetDepthCompare},
    {"getWidth", Wrap_GetWidth},
    {"getHeight", Wrap_GetHeight},
    {"getDepth", Wrap_GetDepth},
    {"getDimensions", Wrap_GetDimensions},
    {"getMipmapCount", Wrap_GetMipmapCount},
    {"getFormat", Wrap_GetFormat},
    {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_texture(lua_State *state) -> int {
  PrintDebug("Registering Texture Lua type.");

  LuaWrap::RegisterLuaType(state, Texture::GetType(),
                           TextureLib); // NOLINT

  return 1;
}

} // namespace Graphics::Texture