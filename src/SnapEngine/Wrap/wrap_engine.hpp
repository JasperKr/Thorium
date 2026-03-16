#pragma once
#include "lua.hpp"

namespace Engine::LuaWrap {

auto RegisterModules(lua_State *state) -> void;

}
