#pragma once

#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Keyboard {

auto Wrap_IsDown(lua_State *state) -> int;
auto Wrap_IsScancodeDown(lua_State *state) -> int;
auto Wrap_SetEnableTextInput(lua_State *state) -> int;
auto Wrap_IsTextInputEnabled(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg KeyboardLib[] = {
    {"isDown", Wrap_IsDown},
    {"isScancodeDown", Wrap_IsScancodeDown},
    {"setEnableTextInput", Wrap_SetEnableTextInput},
    {"isTextInputEnabled", Wrap_IsTextInputEnabled},
    {nullptr, nullptr},
};

// nullptr-terminated
const static lua_CFunction *const childrenInitFunctions = {nullptr};

extern "C" inline auto luaopen_keyboard(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "keyboard",
      .Functions = KeyboardLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}
} // namespace Wrap::Keyboard