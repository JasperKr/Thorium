#pragma once

#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Mouse {

auto Wrap_IsDown(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg MouseLib[] = {
    {"isDown", Wrap_IsDown},
    {nullptr, nullptr},
};

// nullptr-terminated
const static lua_CFunction *const childrenInitFunctions = {nullptr};

extern "C" inline auto luaopen_mouse(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "mouse",
      .Functions = MouseLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}
} // namespace Wrap::Mouse