#pragma once

#include "Modules/console.hpp"
#include "Modules/type.hpp"
#include <Modules/object.hpp>
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
auto PushObject(lua_State *state, const Type *type, Object *object) -> void;
auto RegisterLuaType(lua_State *state, const Type *type,
                     const luaL_Reg *functions) -> void;
auto SetupLuaType(lua_State *state, const Type *type, Object *object) -> void;
auto LoadStorageTable(lua_State *state, const char *key) -> void;

inline auto ProxyFromLua(lua_State *state, int index) -> Proxy * {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    return nullptr;
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  return proxy;
}

template <typename T>
inline auto ObjectFromLua(lua_State *state, int index) -> T * {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    PrintWarning("FromLuaObject: not userdata at index {}", index);
    return nullptr;
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    PrintWarning("FromLuaObject: proxy is null at index {}", index);
    return nullptr;
  }

  if (proxy->object == nullptr || proxy->type == nullptr) {
    PrintWarning("FromLuaObject: proxy invalid at index {}", index);
    return nullptr;
  }

  // Do not compare type addresses, they may differ since the types are
  // Defined inline, so addresses are different for each translation unit
  if (proxy->type->GetName() != T::GetType()->GetName()) {
    auto proxyTypeName = proxy->type->GetName();
    auto expectedTypeName = T::GetType()->GetName();

    PrintWarning(
        "FromLuaObject: type mismatch at index {}, expected: {} got: {}", index,
        expectedTypeName, proxyTypeName);
    return nullptr;
  }

  auto *obj = dynamic_cast<T *>(proxy->object);
  return obj;
}

inline auto PushPointer(lua_State *state, void *pointer) -> void {
  lua_pushlightuserdata(state, pointer);
}

inline auto LuaType(lua_State *state, int index) -> Type const * {
  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr || proxy->object == nullptr) {
    return nullptr;
  }

  return proxy->type;
}

template <typename T> inline auto IsType(lua_State *state, int index) -> bool {
  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr || proxy->object == nullptr) {
    return false;
  }

  return proxy->type->GetName() == T::GetType()->GetName();
}

} // namespace LuaWrap