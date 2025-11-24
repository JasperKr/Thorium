#include "wrap.hpp"
#include <Modules/object.hpp>
#include <iostream>
#include <lauxlib.h>
#include <lua.h>

#include "Wrap/Graphics/graphics.hpp"
#include "Wrap/Modules/event.hpp"
#include "Wrap/Modules/timer.hpp"

#include "Modules/color.hpp"

namespace LuaWrap {

// NOLINTNEXTLINE
static auto wrap__gc(lua_State *state) -> int {
  // NOLINTNEXTLINE
  auto *proxy = (Proxy *)lua_touserdata(state, 1);
  if (proxy->object != nullptr) {
    proxy->object->release();
    proxy->object = nullptr;
  }
  return 0;
}

// NOLINTNEXTLINE
static auto wrap__tostring(lua_State *state) -> int {
  // NOLINTNEXTLINE
  auto *proxy = (Proxy *)lua_touserdata(state, 1);
  lua_pushfstring(state, "%s: %p", proxy->type->GetName().c_str(),
                  static_cast<void *>(proxy->object));
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

auto RegisterLuaModule(lua_State *state, const LuaModule &module) -> void {
  std::cout << "Registering Lua module: " << module.Name << "...\n";
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
  lua_pushcfunction(state, wrap__gc); // [_modules, mt, wrap__gc]
  lua_setfield(state, -2, "__gc");    // mt.__gc = wrap__gc [_modules, mt]

  lua_setmetatable(state, -2); // [_modules, module] Set metatable for userdata
  lua_setfield(state, -2, name); // [_modules] _modules[name] = module
  lua_pop(state, 1);             // []

  std::cout << "Module " << module.Name << " metatable registered."
            << "\n";

  SetStackToTable(state, "Thorium"); // [Thorium]

  lua_newtable(state); // [Thorium, module]

  std::cout << "Module " << module.Name << " registered." << "\n";

  // register Functions to Thorium.modulename.functionname
  if (module.Functions != nullptr) {
    const luaL_Reg *func = module.Functions;
    while (func->name != nullptr) {
      std::cout << "Registering function " << func->name << " in module "
                << module.Name << "\n";
      lua_pushcfunction(state, func->func); // [Thorium, module, func]
      lua_setfield(state, -2, func->name);  // [Thorium, module]
      func++; // NOLINT, functions are nullptr-terminated
    }
  }

  lua_setfield(state, -2, name); // [Thorium]

  std::cout << "Module " << module.Name << " functions registered." << "\n";

  // register init functions
  if (module.ChildrenInitFunctions != nullptr) {
    const lua_CFunction *func = module.ChildrenInitFunctions;
    while (*func != nullptr) {
      (*func)(state);
      func++; // NOLINT, functions are nullptr-terminated
    }
  }

  std::cout << "Module " << module.Name << " children initialized."
            << "\n";

  // done, remove module table from stack
  lua_pop(state, 1); // []

  // Sanity check Thorium[modulename][first function] exists
  lua_getglobal(state, "Thorium");                 // [Thorium]
  lua_getfield(state, -1, module.Name.c_str());    // [Thorium, module]
  lua_getfield(state, -1, module.Functions->name); // [Thorium, module, func]
  if (lua_isfunction(state, -1)) {
    std::cout << ColorText("Registed module successfully", ConsoleColor::Green)
              << ": " << module.Name << "\n";
  }
}

auto RegisterLuaType(lua_State *state, const LuaModule &module) -> void {}

// NOLINTNEXTLINE
static const luaL_Reg ThoriumModules[] = {
    {"graphics", Graphics::luaopen_graphics},
    {"event", Event::luaopen_event},
    {"timer", Timer::luaopen_timer},
    {nullptr, nullptr},
};

auto RegisterModules(lua_State *state) -> void {
  lua_newtable(state);             // [table]
  lua_setglobal(state, "Thorium"); // [Thorium]
  lua_pop(state, 1);               // empty stack

  const luaL_Reg *module = ThoriumModules; // NOLINT
  while (module->name != nullptr) {
    std::cout << "Registering Lua module: " << module->name << "\n";
    module->func(state);
    module++; // NOLINT, modules are nullptr-terminated
  }
}

} // namespace LuaWrap