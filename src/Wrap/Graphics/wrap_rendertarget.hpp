#pragma once

#include "lua.hpp"
namespace Graphics::DynamicRendering {
auto wrap_SetRenderTargets(lua_State *state) -> int;
}