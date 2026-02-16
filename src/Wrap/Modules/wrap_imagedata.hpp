#pragma once

#include "Modules/console.hpp"
#include "Modules/imagedata.hpp"
#include "Wrap/Modules/wrap_bytedata.hpp"
#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Image {

auto wrap_NewImagedata(lua_State *state) -> int;
auto wrap_SetPixel(lua_State *state) -> int;
auto wrap_GetPixel(lua_State *state) -> int;
auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg ImagedataLib[] = {
    {"setUInt32", Data::wrap_SetUInt32},
    {"getUInt32", Data::wrap_GetUInt32},
    {"setInt32", Data::wrap_SetInt32},
    {"getInt32", Data::wrap_GetInt32},
    {"setUInt16", Data::wrap_SetUInt16},
    {"getUInt16", Data::wrap_GetUInt16},
    {"setInt16", Data::wrap_SetInt16},
    {"getInt16", Data::wrap_GetInt16},
    {"setUInt8", Data::wrap_SetUInt8},
    {"getUInt8", Data::wrap_GetUInt8},
    {"setInt8", Data::wrap_SetInt8},
    {"getInt8", Data::wrap_GetInt8},
    {"setFloat", Data::wrap_SetFloat},
    {"getFloat", Data::wrap_GetFloat},
    {"setHalf", Data::wrap_SetHalf},
    {"getHalf", Data::wrap_GetHalf},
    {"setPixel", wrap_SetPixel},
    {"getPixel", wrap_GetPixel},
    {"getWidth", wrap_GetWidth},
    {"getHeight", wrap_GetHeight},
    {"getDimensions", wrap_GetDimensions},
    {"getFormat", wrap_GetFormat},

    {"getSize", Data::wrap_GetSize},
    {"getPointer", Data::wrap_GetPointer},
    {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_imagedata(lua_State *state) -> int {
  PrintDebug("Registering Imagedata Lua type.");

  LuaWrap::RegisterLuaType(state, ::Image::ImageData::GetType(),
                           ImagedataLib); // NOLINT

  return 1;
}

} // namespace Wrap::Image