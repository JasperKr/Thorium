#pragma once

#include "Graphics/texture.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Graphics::Texture {
auto wrap_SetFilter(lua_State *state) -> int;
auto wrap_GetFilter(lua_State *state) -> int;
auto wrap_SetAnisotropy(lua_State *state) -> int;
auto wrap_GetAnisotropy(lua_State *state) -> int;
auto wrap_SetWrapmode(lua_State *state) -> int;
auto wrap_GetWrapmode(lua_State *state) -> int;
auto wrap_SetLodBias(lua_State *state) -> int;
auto wrap_GetLodBias(lua_State *state) -> int;
auto wrap_SetLodRange(lua_State *state) -> int;
auto wrap_GetLodRange(lua_State *state) -> int;
auto wrap_SetDepthCompare(lua_State *state) -> int;
auto wrap_GetDepthCompare(lua_State *state) -> int;
auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDepth(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;
auto wrap_GetMipmapCount(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;
auto wrap_GetID(lua_State *state) -> int; // ImGui texture Identifier
auto wrap_NewTexture(lua_State *state) -> int;
auto wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> TextureLib = {
    {"setFilter", wrap_SetFilter},
    {"getFilter", wrap_GetFilter},
    {"setAnisotropy", wrap_SetAnisotropy},
    {"getAnisotropy", wrap_GetAnisotropy},
    {"setWrap", wrap_SetWrapmode},
    {"getWrap", wrap_GetWrapmode},
    {"setLodBias", wrap_SetLodBias},
    {"getLodBias", wrap_GetLodBias},
    {"setLodRange", wrap_SetLodRange},
    {"getLodRange", wrap_GetLodRange},
    {"setDepthCompare", wrap_SetDepthCompare},
    {"getDepthCompare", wrap_GetDepthCompare},
    {"getWidth", wrap_GetWidth},
    {"getHeight", wrap_GetHeight},
    {"getDepth", wrap_GetDepth},
    {"getDimensions", wrap_GetDimensions},
    {"getMipmapCount", wrap_GetMipmapCount},
    {"getFormat", wrap_GetFormat},
    {"getID", wrap_GetID},

};

extern "C" inline auto luaopen_texture(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Graphics::Texture::GetType(),
                           TextureLib); // NOLINT

  return 1;
}

} // namespace Wrap::Graphics::Texture