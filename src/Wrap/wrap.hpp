#pragma once

#include "Modules/console.hpp"
#include "Modules/error.hpp"
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
auto LoadOrCreateStorageTable(lua_State *state, const char *key) -> void;
auto LogStack(lua_State *state) -> void;

inline auto ProxyFromLua(lua_State *state, int index) -> Proxy * {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    return nullptr;
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));

  if (proxy == nullptr) {
    PrintWarning("ProxyFromLua: proxy is null at index {}", index);
    return nullptr;
  }

  if (proxy->object == nullptr) {
    PrintWarning("ProxyFromLua: proxy invalid object at index {}", index);
    return nullptr;
  }

  if (proxy->type == nullptr) {
    PrintWarning("ProxyFromLua: proxy invalid type at index {}", index);
    return nullptr;
  }

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
  // // NOLINTNEXTLINE
  // auto **userdata =
  //     static_cast<void **>(lua_newuserdata(state, sizeof(void *)));
  // *userdata = pointer;
  lua_pushlightuserdata(state, pointer);
}

inline auto LuaType(lua_State *state, int index) -> Type const * {
  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    PrintWarning("LuaType: proxy is null at index {}", index);
    return nullptr;
  }

  if (proxy->type == nullptr) {
    PrintWarning("LuaType: proxy type is null at index {}", index);
    return nullptr;
  }

  if (proxy->object == nullptr) {
    PrintWarning("LuaType: proxy object is null at index {}", index);
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

inline auto SerializeVarargsToString(lua_State *state, int startIndex)
    -> Result<std::string> {

  constexpr auto encodeVarargs = R"Lua(
  return require('string.buffer').encode({...})
)Lua";

  int numArgs = lua_gettop(state) - startIndex + 1;

  LuaWrap::LoadOrCreateStorageTable(state, "ThoriumObjectStorage"); // [storage]

  // if no function, add it
  lua_getfield(state, -1, "encode arguments"); // [storage, func/nil]

  if (lua_isnil(state, -1)) {
    lua_pop(state, 1); // [storage]

    auto loadResult = luaL_loadstring(state, encodeVarargs); // [storage, func]
    if (loadResult != LUA_OK) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Unexpected(luaErrorMessage);
    }
    lua_pushvalue(state, -1);                    // [storage, func, func]
    lua_setfield(state, -3, "encode arguments"); // [storage, func]
  }

  // [storage, func]

  for (int i = 0; i < numArgs; ++i) {
    // [thread, args, storage, func, arg1, arg2, ...]
    lua_pushvalue(state, startIndex + i);
  }

  auto result = lua_pcall(state, numArgs, 1, 0); // [storage, encodedArgs]

  if (result != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Unexpected(luaErrorMessage);
  }

  size_t len = 0;
  const char *data = lua_tolstring(state, -1, &len);
  std::string launchArguments;
  if (data != nullptr && len > 0) {
    launchArguments.assign(data, len);
  }

  lua_pop(state, 2); // []

  return launchArguments;
}

inline auto PushVarargsFromString(lua_State *state,
                                  const std::string &launchArguments, int count)
    -> Error {
  constexpr auto decodeVarargs = R"Lua(
  local args = require('string.buffer').decode((...))

  return unpack(args, 1, (select(2, ...)))
)Lua";

  LuaWrap::LoadOrCreateStorageTable(state, "ThoriumObjectStorage"); // [storage]

  // if no function, add it
  lua_getfield(state, -1, "decode arguments"); // [storage, func/nil]

  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);                                   // [storage]
    auto result = luaL_loadstring(state, decodeVarargs); // [storage, func]

    if (result != LUA_OK) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Createf("Failed to load decode arguments function: {}",
                            luaErrorMessage);
    }

    lua_pushvalue(state, -1);                    // [storage, func, func]
    lua_setfield(state, -3, "decode arguments"); // [storage, func]
  }

  // [storage, func]

  lua_pushlstring(state, launchArguments.data(),
                  launchArguments.size()); // [storage, func, args]
  lua_pushinteger(state, count);           // [storage, func, args, count]
  auto callResult = lua_pcall(state, 2, count, 0); // [storage, ...]
  if (callResult != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Createf(
        "Failed to push launch arguments in decode arguments: {}",
        luaErrorMessage);
  }

  // [function, args, args...]

  lua_remove(state, 2); // [function, ...]

  return Error::Success();
}

inline auto SerializeValueToString(lua_State *state, int index)
    -> Result<std::string> {
  constexpr auto encodeValue = R"Lua(
  return require('string.buffer').encode({ ... })
)Lua";
  LuaWrap::LoadOrCreateStorageTable(state, "ThoriumObjectStorage"); // [storage]

  // if no function, add it
  lua_getfield(state, -1, "encode value"); // [storage, func/nil]
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);                                     // [storage]
    auto loadResult = luaL_loadstring(state, encodeValue); // [storage, func]
    if (loadResult != LUA_OK) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Unexpectedf("Failed to load encode value function: {}",
                                luaErrorMessage);
    }
    lua_pushvalue(state, -1);                // [storage, func, func]
    lua_setfield(state, -3, "encode value"); // [storage, func]
  }

  // [storage, func]
  lua_pushvalue(state, index);             // [storage, func, value]
  auto result = lua_pcall(state, 1, 1, 0); // [storage, encodedValue]
  if (result != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Unexpectedf("Failed to encode value: {}", luaErrorMessage);
  }

  size_t len = 0;
  const char *data = lua_tolstring(state, -1, &len);
  std::string encodedValue;
  if (data != nullptr && len > 0) {
    encodedValue.assign(data, len);
  }

  lua_pop(state, 2); // []

  return encodedValue;
}

inline auto PushValueFromString(lua_State *state,
                                const std::string &encodedValue) -> Error {
  constexpr auto decodeValue = R"Lua(
  local args = require('string.buffer').decode((...))
  return args[1]
)Lua";

  LuaWrap::LoadOrCreateStorageTable(state, "ThoriumObjectStorage"); // [storage]

  // if no function, add it
  lua_getfield(state, -1, "decode value"); // [storage, func/nil]
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);                                 // [storage]
    auto result = luaL_loadstring(state, decodeValue); // [storage, func]
    if (result != LUA_OK) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Createf("Failed to load decode value function: {}",
                            luaErrorMessage);
    }
    lua_pushvalue(state, -1);                // [storage, func, func]
    lua_setfield(state, -3, "decode value"); // [storage, func]
  }

  // [storage, func]
  lua_pushlstring(state, encodedValue.data(),
                  encodedValue.size());        // [storage, func, encodedValue]
  auto callResult = lua_pcall(state, 1, 1, 0); // [storage, value]
  if (callResult != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Createf("Failed to decode value: {}", luaErrorMessage);
  }

  lua_remove(state, -2); // Remove [storage], leave [value] on stack

  return Error::Success();
}

} // namespace LuaWrap