#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Mouse {

auto Wrap_IsDown(lua_State *state) -> int;
auto Wrap_GetX(lua_State *state) -> int;
auto Wrap_GetY(lua_State *state) -> int;
auto Wrap_GetPosition(lua_State *state) -> int;
auto Wrap_SetPosition(lua_State *state) -> int;
auto Wrap_SetX(lua_State *state) -> int;
auto Wrap_SetY(lua_State *state) -> int;
auto Wrap_SetRelativeMode(lua_State *state) -> int;
auto Wrap_GetRelativeMode(lua_State *state) -> int;
auto Wrap_SetVisible(lua_State *state) -> int;
auto Wrap_GetVisible(lua_State *state) -> int;
auto Wrap_GetHardwareCursor(lua_State *state) -> int;
auto Wrap_NewCursor(lua_State *state) -> int;
auto Wrap_SetCursor(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> MouseLib = {
    {"isDown", Wrap_IsDown},
    {"getX", Wrap_GetX},
    {"getY", Wrap_GetY},
    {"getPosition", Wrap_GetPosition},
    {"setPosition", Wrap_SetPosition},
    {"setX", Wrap_SetX},
    {"setY", Wrap_SetY},
    {"setRelativeMode", Wrap_SetRelativeMode},
    {"getRelativeMode", Wrap_GetRelativeMode},
    {"setVisible", Wrap_SetVisible},
    {"getVisible", Wrap_GetVisible},
    {"getHardwareCursor", Wrap_GetHardwareCursor},
    {"newCursor", Wrap_NewCursor},
    {"setCursor", Wrap_SetCursor},

};

// nullptr-terminated
const static std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_mouse(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "mouse",
      .Functions = MouseLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,

  };

  RegisterLuaModule(state, module);
  return 1;
}
} // namespace Wrap::Mouse