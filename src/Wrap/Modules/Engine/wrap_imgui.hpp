#pragma once

#include "Modules/error.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Imgui {
auto NewFrame(lua_State *state) -> int;
auto EndFrame(lua_State *state) -> int;
auto Draw(lua_State *state) -> int;

// Imgui event passthrough functions
auto MousePressed(lua_State *state) -> int;
auto MouseReleased(lua_State *state) -> int;
auto MouseMoved(lua_State *state) -> int;
auto MouseWheelMoved(lua_State *state) -> int;

auto KeyPressed(lua_State *state) -> int;
auto KeyReleased(lua_State *state) -> int;
auto TextInput(lua_State *state) -> int;

auto Shutdown() -> Error;

// NOLINTNEXTLINE
static const luaL_Reg ImGuiLib[] = {
    {"newFrame", NewFrame},
    {"endFrame", EndFrame},
    {"draw", Draw},
    {"mousePressed", MousePressed},
    {"mouseReleased", MouseReleased},
    {"mouseMoved", MouseMoved},
    {"mouseWheelMoved", MouseWheelMoved},
    {"keyPressed", KeyPressed},
    {"keyReleased", KeyReleased},
    {"textInput", TextInput},
    {nullptr, nullptr},
};

// nullptr-terminated NOLINTNEXTLINE
const static lua_CFunction childrenInitFunctions[] = {
    nullptr,
};

extern "C" inline auto luaopen_gui(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "gui",
      .Functions = ImGuiLib,                          // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Imgui