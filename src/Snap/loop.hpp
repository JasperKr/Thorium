#pragma once

#include "Modules/error.hpp"
#include "lua.hpp"
#include <string>
#include <vector>

auto MainLoop(const std::vector<std::string> &arguments) -> Error;
auto RegisterAllLuaModules(lua_State *state) -> void;