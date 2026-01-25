#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
namespace Graphics::DynamicRendering {
auto wrap_SetRenderTargets(lua_State *state) -> int;
}