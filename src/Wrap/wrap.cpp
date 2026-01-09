#include "wrap.hpp"
#include "Modules/console.hpp"
#include "Wrap/Modules/Editor/wrap_imgui.hpp"
#include "Wrap/Modules/wrap_data.hpp"
#include "Wrap/Modules/wrap_mouse.hpp"

#include <cstdint>
#include <iostream>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "Modules/object.hpp"
#include "Wrap/Graphics/wrap_graphics.hpp"
#include "Wrap/Modules/wrap_event.hpp"
#include "Wrap/Modules/wrap_filesystem.hpp"
#include "Wrap/Modules/wrap_keyboard.hpp"
#include "Wrap/Modules/wrap_timer.hpp"

namespace LuaWrap {

static auto wrap_gc(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);
  if (proxy->object != nullptr) {
    proxy->object->release();
    proxy->object = nullptr;
  }
  return 0;
}

// NOLINTNEXTLINE
static auto wrap_tostring(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);
  lua_pushfstring(state, "%s: %p", proxy->type->GetName().c_str(),
                  static_cast<void *>(proxy->object));
  return 1;
}

static auto wrap_type(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);

  if (proxy == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushstring(state, proxy->type->GetName().c_str());
  return 1;
}

static auto wrap_typeof(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);
  const auto *typeName = luaL_checkstring(state, 2);
  bool sameType = (proxy->type->GetName() == typeName);
  lua_pushboolean(state, sameType ? 1 : 0);
  return 1;
}

static auto wrap_eq(lua_State *state) -> int {
  Proxy *proxyA = ProxyFromLua(state, 1);
  Proxy *proxyB = ProxyFromLua(state, 2);
  bool isEqual = (proxyA->object == proxyB->object);
  lua_pushboolean(state, isEqual ? 1 : 0);
  return 1;
}

static auto wrap_release(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);
  Object *object = proxy->object;

  if (object != nullptr) {
    proxy->object = nullptr;
    object->release();

    // load object storage table
    LoadStorageTable(state, "ThoriumObjectStorage"); // [storage]

    // NOLINTNEXTLINE
    auto key = (uintptr_t)(object);

    // NOLINTNEXTLINE
    lua_pushlightuserdata(state, (void *)key); // [storage, key]
    lua_pushnil(state);                        // [storage, key, nil]
    lua_settable(state, -3);                   // storage[key] = nil  [storage]

    lua_pop(state, 1); // []
  }

  // Success if object is now null
  lua_pushboolean(state, proxy->object == nullptr ? 1 : 0);

  return 1;
}

auto SetStackToTable(lua_State *state, const char *key) -> void {
  lua_getglobal(state, key);

  if (!lua_istable(state, -1)) { // Not found
    lua_pop(state, 1);           // Remove non-table value
    lua_newtable(state);
    lua_pushvalue(state, -1);
    lua_setglobal(state, key);
  }
}

auto LoadStorageTable(lua_State *state, const char *key) -> void {
  lua_getfield(state, LUA_REGISTRYINDEX, key);
}

auto LoadOrCreateStorageTable(lua_State *state, const char *key) -> void {
  lua_getfield(state, LUA_REGISTRYINDEX, key); // [storage]

  if (!lua_istable(state, -1)) {
    // Create storage table
    lua_newtable(state);
    lua_replace(state, -2); // replace nil in registry with new table

    // Create metatable with weak values
    lua_newtable(state);
    lua_pushstring(state, "v");
    lua_setfield(state, -2, "__mode");
    lua_setmetatable(state, -2); // new table .mt = mt, popped mt

    // we now only have storage and new table
    lua_setfield(state, LUA_REGISTRYINDEX, key);
  }
}

auto RegisterLuaModule(lua_State *state, const LuaModule &module) -> void {
  if (module.Functions == nullptr) {
    std::cerr << "Module " << module.Name << " has no functions to register."
              << "\n";
    return;
  }

  SetStackToTable(state, "_modules"); // [_modules]

  // Create userdata for the module NOLINTNEXTLINE
  auto *proxy = (Proxy *)lua_newuserdata(state, sizeof(Proxy));
  proxy->type = module.ModuleType;
  // proxy->object = m.module; // TODO: idk...

  const auto *name = module.Name.c_str();

  luaL_newmetatable(state, name);     // Create metatable [_modules, mt]
  lua_pushvalue(state, -1);           // Duplicate metatable [_modules, mt, mt]
  lua_setfield(state, -2, "__index"); // mt.__index = mt [_modules, mt]
  lua_pushcfunction(state, wrap_gc);  // [_modules, mt, wrap__gc]
  lua_setfield(state, -2, "__gc");    // mt.__gc = wrap__gc [_modules, mt]

  lua_setmetatable(state, -2); // [_modules, module] Set metatable for userdata
  lua_setfield(state, -2, name); // [_modules] _modules[name] = module
  lua_pop(state, 1);             // []

  SetStackToTable(state, "Thorium"); // [Thorium]

  lua_newtable(state); // [Thorium, module]

  // register Functions to Thorium.modulename.functionname
  if (module.Functions != nullptr) {
    const luaL_Reg *func = module.Functions;
    while (func->name != nullptr) {
      lua_pushcfunction(state, func->func); // [Thorium, module, func]
      lua_setfield(state, -2, func->name);  // [Thorium, module]
      func++; // NOLINT, functions are nullptr-terminated
    }
  }

  lua_setfield(state, -2, name); // [Thorium]

  // register init functions
  if (module.ChildrenInitFunctions != nullptr) {
    const lua_CFunction *func = module.ChildrenInitFunctions;
    while (*func != nullptr) {
      (*func)(state);
      func++; // NOLINT, functions are nullptr-terminated
    }
  }

  // done, remove module table from stack
  lua_pop(state, 1); // []

  // Sanity check Thorium[modulename][first function] exists
  lua_getglobal(state, "Thorium"); // [Thorium]
  if (lua_isnil(state, -1)) {
    std::cerr << "Error registering module " << module.Name
              << ": Thorium table not found." << "\n";
    lua_pop(state, 1); // []
    return;
  }
  lua_getfield(state, -1, module.Name.c_str()); // [Thorium, module]
  if (lua_isnil(state, -1)) {
    std::cerr << "Error registering module " << module.Name
              << ": module table not found." << "\n";
    lua_pop(state, 2); // []
    return;
  }
  if (module.Functions->name == nullptr) {
    PrintWarning("Module {} has no functions to verify during registration.",
                 module.Name);
    lua_pop(state, 2); // []
    return;
  }
  lua_getfield(state, -1, module.Functions->name); // [Thorium, module, func]
  if (lua_isnil(state, -1)) {
    std::cerr << "Error registering module " << module.Name << ": function "
              << module.Functions->name << " not found." << "\n";
    lua_pop(state, 3); // []
    return;
  }
}

auto RegisterLuaType(lua_State *state, const Type *type,
                     const luaL_Reg *functions) -> void {

  if (type == nullptr) {
    std::cerr << "Cannot register Lua type: type is null" << "\n";
    return;
  }

  // Make sure permanent object storage table exists with weak values
  LoadOrCreateStorageTable(state, "ThoriumObjectStorage"); // [storage]

  lua_pop(state, 1); // Remove storage table from stack []

  const auto *name = type->GetName().c_str();

  luaL_newmetatable(state, name);     // Create metatable [mt]
  lua_pushvalue(state, -1);           // Duplicate metatable [mt, mt]
  lua_setfield(state, -2, "__index"); // mt.__index = mt [mt]
  lua_pushcfunction(state, wrap_gc);  // [mt, wrap__gc]
  lua_setfield(state, -2, "__gc");    // mt.__gc = wrap__gc [mt]
  lua_pushcfunction(state, wrap_tostring);
  lua_setfield(state, -2, "__tostring"); // mt.__tostring = wrap__tostring [mt]
  lua_pushcfunction(state, wrap_type);
  lua_setfield(state, -2, "type"); // mt.type = wrap_type [mt]
  lua_pushcfunction(state, wrap_typeof);
  lua_setfield(state, -2, "typeof"); // mt.typeof = wrap_typeof [mt]
  lua_pushcfunction(state, wrap_eq);
  lua_setfield(state, -2, "__eq"); // mt.__eq = wrap_eq [mt]
  lua_pushcfunction(state, wrap_release);
  lua_setfield(state, -2, "release"); // mt.release = wrap_release [mt]

  // Register functions
  if (functions != nullptr) {
    const luaL_Reg *func = functions;
    while (func->name != nullptr) {
      lua_pushcfunction(state, func->func); // [mt, func]
      lua_setfield(state, -2, func->name);  // [mt]
      func++; // NOLINT, functions are nullptr-terminated
    }
  }

  lua_pop(state, 1); // []
}

auto SetupLuaType(lua_State *state, const Type *type, Object *object) -> void {
  // NOLINTNEXTLINE
  auto *proxy = (Proxy *)lua_newuserdata(state, sizeof(Proxy)); // [userdata]
  proxy->type = type;
  proxy->object = object;

  object->retain();

  const auto *name = type->GetName().c_str();

  luaL_newmetatable(state, name); // Get metatable, [userdata, mt]

  lua_getfield(state, -1, "__gc");
  bool hasGC = (lua_isfunction(state, -1) != 0);
  lua_pop(state, 1);

  if (!hasGC) {
    // Add GC function
    lua_pushcfunction(state, wrap_gc); // [userdata, mt, wrap__gc]
    lua_setfield(state, -2, "__gc");   // mt.__gc = wrap__gc [userdata, mt]
  }

  lua_setmetatable(state, -2); // Set metatable for userdata, [userdata]
}

auto PushObject(lua_State *state, const Type *type, Object *object) -> void {
  if (object == nullptr) {
    lua_pushnil(state);
    return;
  }

  // fetch permanent object storage table
  LoadStorageTable(state, "ThoriumObjectStorage"); // [storage]

  if (lua_isnoneornil(state, -1)) {
    // No storage table

    lua_pop(state, 1); // Remove nil [empty]

    SetupLuaType(state, type, object); // [userdata]
    return;
  }

  // Check if object already has a userdata
  auto key = (uintptr_t)(object);            // NOLINT
  lua_pushlightuserdata(state, (void *)key); // [storage, key] NOLINT
  lua_gettable(state, -2);                   // [storage, value]

  if (lua_type(state, -1) != LUA_TUSERDATA) {
    // No existing userdata
    lua_pop(state, 1); // Remove nil [storage]

    // Create new userdata
    SetupLuaType(state, type, object); // [storage, userdata]

    // Store in storage table
    lua_pushlightuserdata(state,
                          (void *)key); // [storage, userdata, key] NOLINT
    lua_pushvalue(state, -2);           // [storage, userdata, key, userdata]
    lua_settable(state, -4); // storage[key] = userdata [storage, userdata]
  }

  lua_remove(state, -2); // Remove storage table [userdata]
}

auto PushObject(lua_State *state, Object *object) -> void {
  if (object == nullptr) {
    lua_pushnil(state);
    return;
  }

  // fetch permanent object storage table
  LoadStorageTable(state, "ThoriumObjectStorage"); // [storage]

  if (lua_isnoneornil(state, -1)) {
    // No storage table

    lua_pop(state, 1); // Remove nil [empty]

    SetupLuaType(state, object->GetInstanceType(), object); // [userdata]
    return;
  }

  // Check if object already has a userdata
  auto key = (uintptr_t)(object);            // NOLINT
  lua_pushlightuserdata(state, (void *)key); // [storage, key] NOLINT
  lua_gettable(state, -2);                   // [storage, value]

  if (lua_type(state, -1) != LUA_TUSERDATA) {
    // No existing userdata
    lua_pop(state, 1); // Remove nil [storage]

    // Create new userdata
    SetupLuaType(state, object->GetInstanceType(),
                 object); // [storage, userdata]

    // Store in storage table
    lua_pushlightuserdata(state,
                          (void *)key); // [storage, userdata, key] NOLINT
    lua_pushvalue(state, -2);           // [storage, userdata, key, userdata]
    lua_settable(state, -4); // storage[key] = userdata [storage, userdata]
  }

  lua_remove(state, -2); // Remove storage table [userdata]
}

// NOLINTNEXTLINE
static const luaL_Reg ThoriumModules[] = {
    {"graphics", Graphics::luaopen_graphics},
    {"event", Event::luaopen_event},
    {"timer", Timer::luaopen_timer},
    {"data", Data::luaopen_data},
    {"filesystem", Wrap::Filesystem::luaopen_filesystem},
    {"keyboard", Wrap::Keyboard::luaopen_keyboard},
    {"mouse", Wrap::Mouse::luaopen_mouse},
    {"gui", Wrap::Imgui::luaopen_gui},
    {nullptr, nullptr},
};

auto RegisterModules(lua_State *state) -> void {
  lua_newtable(state);             // [table]
  lua_setglobal(state, "Thorium"); // [Thorium]
  lua_pop(state, 1);               // empty stack

  const luaL_Reg *module = ThoriumModules; // NOLINT
  while (module->name != nullptr) {
    module->func(state);
    module++; // NOLINT, modules are nullptr-terminated
  }
}

} // namespace LuaWrap