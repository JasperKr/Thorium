#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Window {

auto wrap_Hide(lua_State *state) -> int;
auto wrap_GetDisplayCount(lua_State *state) -> int;
auto wrap_GetDisplayName(lua_State *state) -> int;
auto wrap_IsFullscreen(lua_State *state) -> int;
auto wrap_GetFullscreenDimensions(lua_State *state) -> int;
auto wrap_SetFullscreen(lua_State *state) -> int;
auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;
auto wrap_SetDimensions(lua_State *state) -> int;
auto wrap_SetIcon(lua_State *state) -> int;
auto wrap_GetSettings(lua_State *state) -> int;
auto wrap_SetSettings(lua_State *state) -> int;
auto wrap_UpdateSettings(lua_State *state) -> int;
auto wrap_SetInitialSettings(lua_State *state) -> int;
auto wrap_GetTitle(lua_State *state) -> int;
auto wrap_SetTitle(lua_State *state) -> int;
auto wrap_GetPosition(lua_State *state) -> int;
auto wrap_SetPosition(lua_State *state) -> int;
auto wrap_SetVSync(lua_State *state) -> int;
auto wrap_HasMouseFocus(lua_State *state) -> int;
auto wrap_HasKeyboardFocus(lua_State *state) -> int;
auto wrap_IsVisible(lua_State *state) -> int;
auto wrap_IsOpen(lua_State *state) -> int;
auto wrap_IsMinimized(lua_State *state) -> int;
auto wrap_IsMaximized(lua_State *state) -> int;
auto wrap_Minimize(lua_State *state) -> int;
auto wrap_Maximize(lua_State *state) -> int;
auto wrap_Restore(lua_State *state) -> int;
auto wrap_EnableDisplaySleep(lua_State *state) -> int;
auto wrap_IsDisplaySleepEnabled(lua_State *state) -> int;
auto wrap_RequestAttention(lua_State *state) -> int;
auto wrap_SetColorSpace(lua_State *state) -> int;
auto wrap_GetColorSpace(lua_State *state) -> int;

static const std::vector<luaL_Reg> WindowLib = {
    {"hide", wrap_Hide},
    {"getDisplayCount", wrap_GetDisplayCount},
    {"getDisplayName", wrap_GetDisplayName},
    {"isFullscreen", wrap_IsFullscreen},
    {"getFullscreenDimensions", wrap_GetFullscreenDimensions},
    {"setFullscreen", wrap_SetFullscreen},
    {"getWidth", wrap_GetWidth},
    {"getHeight", wrap_GetHeight},
    {"getDimensions", wrap_GetDimensions},
    {"setDimensions", wrap_SetDimensions},
    {"setIcon", wrap_SetIcon},
    {"getSettings", wrap_GetSettings},
    {"setSettings", wrap_SetSettings},
    {"updateSettings", wrap_UpdateSettings},
    {"getTitle", wrap_GetTitle},
    {"setTitle", wrap_SetTitle},
    {"getPosition", wrap_GetPosition},
    {"setPosition", wrap_SetPosition},
    {"setVSync", wrap_SetVSync},
    {"hasMouseFocus", wrap_HasMouseFocus},
    {"hasKeyboardFocus", wrap_HasKeyboardFocus},
    {"isVisible", wrap_IsVisible},
    {"isOpen", wrap_IsOpen},
    {"isMinimized", wrap_IsMinimized},
    {"isMaximized", wrap_IsMaximized},
    {"minimize", wrap_Minimize},
    {"maximize", wrap_Maximize},
    {"restore", wrap_Restore},
    {"enableDisplaySleep", wrap_EnableDisplaySleep},
    {"isDisplaySleepEnabled", wrap_IsDisplaySleepEnabled},
    {"requestAttention", wrap_RequestAttention},
    {"setColorSpace", wrap_SetColorSpace},
    {"getColorSpace", wrap_GetColorSpace},

};

static const std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_window(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "window",
      .Functions = WindowLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,

  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Window