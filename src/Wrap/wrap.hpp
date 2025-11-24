#pragma once

#include "Modules/type.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace LuaWrap {

struct LuaModule {
  // Name of the module
  std::string Name;

  // Must be nullptr-terminated
  const luaL_Reg *Functions{nullptr};

  // Must be nullptr-terminated, always a leaf. No modules in modules.
  const lua_CFunction *ChildrenInitFunctions{nullptr};

  // Associated Type instance
  Type *ModuleType{nullptr};
};

auto RegisterLuaModule(lua_State *state, const LuaModule &module) -> void;
auto RegisterModules(lua_State *state) -> void;

// Sets the stack to a table stored in the registry under the given key
inline auto SetStackToRegistry(lua_State *state, const char *key) -> void {
  lua_getfield(state, LUA_REGISTRYINDEX, key);

  if (!lua_istable(state, -1)) { // Not found
    lua_pop(state, 1);           // Remove non-table value
    lua_newtable(state);
    lua_pushvalue(state, -1);
    lua_setfield(state, LUA_REGISTRYINDEX, key);
  }
}

auto SetStackToTable(lua_State *state, const char *key) -> void;

} // namespace LuaWrap