#pragma once

#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/type.hpp"
#include "Wrap/lua_data.hpp"
#include "lua.hpp"
#include <Modules/object.hpp>
#include <variant>
#include <vector>

namespace LuaWrap {

struct LuaModule {
  // Name of the module
  std::string Name;

  std::vector<luaL_Reg> Functions;
  std::vector<lua_CFunction> ChildrenInitFunctions;
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
                     const std::vector<luaL_Reg> &functions) -> void;
auto SetupLuaType(lua_State *state, const Type *type, Object *object) -> void;
auto LoadStorageTable(lua_State *state, const char *key) -> void;
auto LoadOrCreateStorageTable(lua_State *state, const char *key) -> void;
auto LogStack(lua_State *state) -> void;

// Test if T is a valid Lua userdata C++ object usable with a proxy.
template <typename T>
concept LuaObject = std::is_base_of_v<Object, std::remove_cvref_t<T>>;

template <typename T>
  requires LuaObject<T>
inline auto ObjectFromLua(lua_State *state, int index) -> T * {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
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

  auto *obj = static_cast<T *>(proxy->object);
  return obj;
}

template <typename T>
  requires LuaObject<T>
inline auto ResultFromLua(lua_State *state, int index) -> Result<T *> {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    return Error::Unexpected("Expected userdata at index " +
                             std::to_string(index));
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    return Error::Unexpected("Proxy is null at index " + std::to_string(index));
  }

  if (proxy->object == nullptr || proxy->type == nullptr) {
    return Error::Unexpected("Proxy is invalid at index " +
                             std::to_string(index));
  }

  // Do not compare type addresses, they may differ since the types are
  // Defined inline, so addresses are different for each translation unit
  if (proxy->type->GetName() != T::GetType()->GetName()) {
    auto proxyTypeName = proxy->type->GetName();
    auto expectedTypeName = T::GetType()->GetName();

    return Error::Unexpected("Type mismatch at index " + std::to_string(index) +
                             ", expected: " + expectedTypeName +
                             " got: " + proxyTypeName);
  }

  auto *obj = static_cast<T *>(proxy->object);
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

inline auto SerializeVarargs(lua_State *state, int startIndex)
    -> Result<std::vector<Data::LuaType>> {

  int numArgs = lua_gettop(state) - startIndex + 1;

  std::vector<Data::LuaType> launchArguments;
  launchArguments.reserve(static_cast<size_t>(numArgs));

  for (int i = 0; i < numArgs; ++i) {
    if (lua_isnoneornil(state, startIndex + i)) {
      launchArguments.emplace_back(std::monostate{});
      continue;
    }

    auto argResult = Data::FromStack(state, startIndex + i);
    if (Error::IsError(argResult)) {
      return argResult.error().AsUnexpected();
    }
    launchArguments.emplace_back(argResult.value());
  }

  return launchArguments;
}

inline auto PushVarargs(lua_State *state,
                        const std::vector<Data::LuaType> &launchArguments,
                        int count) -> Error {

  for (int i = 0; i < count; ++i) {
    const auto &arg = launchArguments[static_cast<size_t>(i)];

    auto pushResult = Data::ToStack(state, arg);
    if (Error::IsError(pushResult)) {
      return pushResult;
    }
  }

  return Error::Success();
}

} // namespace LuaWrap