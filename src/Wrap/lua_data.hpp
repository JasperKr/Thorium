#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <string>
#include <variant>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace LuaWrap::Data {

using LuaType =
    std::variant<bool, double, std::string, Proxy, std::vector<struct LuaData>>;

struct LuaData {
  LuaType key;
  LuaType value;
};

auto FromStack(lua_State *state, int index) -> Result<LuaType>;
auto ToStack(lua_State *state, const LuaType &data) -> Error;

} // namespace LuaWrap::Data