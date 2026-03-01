#pragma once

#include "Graphics/Buffers/structured.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
namespace Graphics {

auto wrap_GetSize(lua_State *state) -> int;
auto wrap_GetElementCount(lua_State *state) -> int;
auto wrap_GetElementStride(lua_State *state) -> int;
auto wrap_ClearBuffer(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;
auto wrap_SetData(lua_State *state) -> int;
auto wrap_CopyTo(lua_State *state) -> int;
auto wrap_GetComponentOffset(lua_State *state) -> int;
auto wrap_GetDebugName(lua_State *state) -> int;

auto wrap_NewBuffer(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg BufferLib[] = {
    {"getSize", wrap_GetSize},
    {"getElementCount", wrap_GetElementCount},
    {"getElementStride", wrap_GetElementStride},
    {"getFormat", wrap_GetFormat},
    {"setData", wrap_SetData},
    {"clear", wrap_ClearBuffer},
    {"copyTo", wrap_CopyTo},
    {"getComponentOffset", wrap_GetComponentOffset},
    {"getDebugName", wrap_GetDebugName},
    {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_buffer(lua_State *state) -> int {
  PrintDebug("Registering Buffer Lua type.");

  LuaWrap::RegisterLuaType(state, Graphics::StructuredBuffer::GetType(),
                           BufferLib); // NOLINT

  return 1;
}
} // namespace Graphics