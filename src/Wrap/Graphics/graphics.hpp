#pragma once

#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
namespace Graphics {

auto wrap_Present(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg GraphicsLib[] = {
    {"present", wrap_Present},
    {nullptr, nullptr},
};

// nullptr-terminated
const static lua_CFunction *const childrenInitFunctions = {nullptr};

extern "C" inline auto luaopen_graphics(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "graphics",
      .Functions = GraphicsLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Graphics