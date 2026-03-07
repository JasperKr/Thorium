#pragma once

#include "Modules/bytedata.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Data {

auto wrap_SetUInt32(lua_State *state) -> int;
auto wrap_SetInt32(lua_State *state) -> int;
auto wrap_SetUInt16(lua_State *state) -> int;
auto wrap_SetInt16(lua_State *state) -> int;
auto wrap_SetUInt8(lua_State *state) -> int;
auto wrap_SetInt8(lua_State *state) -> int;

auto wrap_SetFloat(lua_State *state) -> int;
auto wrap_SetHalf(lua_State *state) -> int;

auto wrap_GetUInt32(lua_State *state) -> int;
auto wrap_GetInt32(lua_State *state) -> int;
auto wrap_GetUInt16(lua_State *state) -> int;
auto wrap_GetInt16(lua_State *state) -> int;
auto wrap_GetUInt8(lua_State *state) -> int;
auto wrap_GetInt8(lua_State *state) -> int;

auto wrap_GetFloat(lua_State *state) -> int;
auto wrap_GetHalf(lua_State *state) -> int;

auto wrap_GetSize(lua_State *state) -> int;
auto wrap_GetPointer(lua_State *state) -> int;

auto wrap_NewBytedata(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg BytedataLib[] = {
    {"setUInt32", wrap_SetUInt32},
    {"getUInt32", wrap_GetUInt32},
    {"setInt32", wrap_SetInt32},
    {"getInt32", wrap_GetInt32},
    {"setUInt16", wrap_SetUInt16},
    {"getUInt16", wrap_GetUInt16},
    {"setInt16", wrap_SetInt16},
    {"getInt16", wrap_GetInt16},
    {"setUInt8", wrap_SetUInt8},
    {"getUInt8", wrap_GetUInt8},
    {"setInt8", wrap_SetInt8},
    {"getInt8", wrap_GetInt8},
    {"setFloat", wrap_SetFloat},
    {"getFloat", wrap_GetFloat},
    {"setHalf", wrap_SetHalf},
    {"getHalf", wrap_GetHalf},

    {"getSize", wrap_GetSize},
    {"getPointer", wrap_GetPointer},
    {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_bytedata(lua_State *state) -> int {
  PrintDebug("Registering Bytedata Lua type.");

  LuaWrap::RegisterLuaType(state, ::Data::ByteData::GetType(),
                           BytedataLib); // NOLINT

  return 1;
}

} // namespace Wrap::Data