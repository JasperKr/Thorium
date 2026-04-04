#pragma once

#include "Graphics/Buffers/structured.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Graphics::Buffer {

auto wrap_GetSize(lua_State *state) -> int;
auto wrap_GetElementCount(lua_State *state) -> int;
auto wrap_GetElementStride(lua_State *state) -> int;
auto wrap_ClearBuffer(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;
auto wrap_SetData(lua_State *state) -> int;
auto wrap_GetComponentOffset(lua_State *state) -> int;
auto wrap_GetDebugName(lua_State *state) -> int;
auto wrap_BufferHasPadding(lua_State *state) -> int;

auto wrap_NewBuffer(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> BufferLib = {
    {"getSize", wrap_GetSize},
    {"getElementCount", wrap_GetElementCount},
    {"getElementStride", wrap_GetElementStride},
    {"getFormat", wrap_GetFormat},
    {"setData", wrap_SetData},
    {"clear", wrap_ClearBuffer},
    {"getComponentOffset", wrap_GetComponentOffset},
    {"getDebugName", wrap_GetDebugName},
    {"hasPadding", wrap_BufferHasPadding},

};

extern "C" inline auto luaopen_buffer(lua_State *state) -> int {
  PrintDebug("Registering Buffer Lua type.");

  LuaWrap::RegisterLuaType(state, ::Graphics::StructuredBuffer::GetType(),
                           BufferLib); // NOLINT

  return 1;
}
} // namespace Wrap::Graphics::Buffer