#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Event {

auto wrap_Quit(lua_State *state) -> int;
auto wrap_Pull(lua_State *state) -> int;
auto wrap_Pop(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> EventLib = {
    {"pull", wrap_Pull},
    {"pop", wrap_Pop},
    {"quit", wrap_Quit},

};

const static std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_event(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "event",
      .Functions = EventLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,

  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Event