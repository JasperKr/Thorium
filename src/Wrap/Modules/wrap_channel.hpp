#pragma once

#include "Modules/channel.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Threading {

auto wrap_NewChannel(lua_State *state) -> int;
auto wrap_Push(lua_State *state) -> int;
auto wrap_Pop(lua_State *state) -> int;
auto wrap_Peek(lua_State *state) -> int;
auto wrap_GetCount(lua_State *state) -> int;
auto wrap_Demand(lua_State *state) -> int;
auto wrap_Clear(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ChannelLib = {
    {"push", wrap_Push},         {"pop", wrap_Pop},       {"peek", wrap_Peek},
    {"getCount", wrap_GetCount}, {"demand", wrap_Demand}, {"clear", wrap_Clear},

};

extern "C" inline auto luaopen_channel(lua_State *state) -> int {
  PrintDebug("Registering Channel Lua type.");

  LuaWrap::RegisterLuaType(state, ::Threading::Channel::GetType(),
                           ChannelLib); // NOLINT

  return 1;
}

} // namespace Wrap::Threading