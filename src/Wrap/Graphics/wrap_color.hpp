#pragma once

#include "Modules/color.hpp"
#include <cstdint>
#include <lauxlib.h>
#include <lua.h>

enum ColorFormat : uint8_t {
  List,
  VarArg,
};

static inline auto ColorFromLuaState(lua_State *state, ColorFormat format)
    -> Color {
  switch (format) {
  case ColorFormat::List: {
    luaL_checktype(state, 1, LUA_TTABLE);
    auto red = static_cast<float>(lua_tonumber(state, -4));
    auto green = static_cast<float>(lua_tonumber(state, -3));
    auto blue = static_cast<float>(lua_tonumber(state, -2));
    auto alpha = static_cast<float>(lua_tonumber(state, -1));

    return {red, green, blue, alpha};
  }
  case ColorFormat::VarArg: {
    auto red = static_cast<float>(luaL_checknumber(state, 1));
    auto green = static_cast<float>(luaL_checknumber(state, 2));
    auto blue = static_cast<float>(luaL_checknumber(state, 3));
    auto alpha = static_cast<float>(luaL_checknumber(state, 4));

    return {red, green, blue, alpha};
  }
  }

  return Color{}; // Impossible to reach but windows compiler is stupid af
}