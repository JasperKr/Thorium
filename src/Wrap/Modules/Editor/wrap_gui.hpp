#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Gui {

auto DrawEngineUIComponent(lua_State *state) -> int;
} // namespace Wrap::Gui