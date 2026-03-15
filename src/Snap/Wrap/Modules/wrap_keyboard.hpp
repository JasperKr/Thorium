#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Keyboard {

auto Wrap_IsDown(lua_State *state) -> int;
auto Wrap_IsScancodeDown(lua_State *state) -> int;
auto Wrap_SetEnableTextInput(lua_State *state) -> int;
auto Wrap_IsTextInputEnabled(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> KeyboardLib = {
    {"isDown", Wrap_IsDown},
    {"isScancodeDown", Wrap_IsScancodeDown},
    {"setEnableTextInput", Wrap_SetEnableTextInput},
    {"isTextInputEnabled", Wrap_IsTextInputEnabled},

};

const static std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_keyboard(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "keyboard",
      .Functions = KeyboardLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,

  };

  RegisterLuaModule(state, module);
  return 1;
}
} // namespace Wrap::Keyboard