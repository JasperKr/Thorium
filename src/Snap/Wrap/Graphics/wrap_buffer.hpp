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
auto wrap_Readback(lua_State *state) -> int;

auto wrap_NewBuffer(lua_State *state) -> int;

auto wrap_Readback_GetData(lua_State *state) -> int;
auto wrap_Readback_IsReady(lua_State *state) -> int;
auto wrap_Readback_GetError(lua_State *state) -> int;
auto wrap_Readback_Wait(lua_State *state) -> int;

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

static const std::vector<luaL_Reg> BufferReadbackLib = {
    {"getData", wrap_Readback_GetData},
    {"isReady", wrap_Readback_IsReady},
    {"getError", wrap_Readback_GetError},
    {"wait", wrap_Readback_Wait},
};

static const LuaWrap::LuaClass BufferReadbackClass{
    .Name = "BufferReadback",
    .Type = ::Graphics::BufferReadback::GetType(),
    .Methods = BufferReadbackLib,
    .Children = {},
};

static const LuaWrap::LuaClass BufferClass{
    .Name = "Buffer",
    .Type = ::Graphics::StructuredBuffer::GetType(),
    .Methods = BufferLib,
    .Children = {BufferReadbackClass},
};

extern "C" inline auto luaopen_buffer(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, BufferClass);

  return 1;
}
} // namespace Wrap::Graphics::Buffer