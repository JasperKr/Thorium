#include "config.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Config {

// NOLINTNEXTLINE
static ApplicationConfig globalConfig;

inline auto SetVsync(lua_State *state) -> int {
  if (!lua_isboolean(state, 1)) {
    return luaL_error(state, "Expected boolean for Vsync");
  }
  globalConfig.Vsync = (lua_toboolean(state, 1) != 0);
  PrintInfo("Vsync set to ", (globalConfig.Vsync ? "true" : "false"));
  return 0;
}

inline auto SetTitle(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for Title");
  }
  globalConfig.Title = lua_tostring(state, 1);
  PrintInfo("Title set to ", globalConfig.Title);
  return 0;
}

inline auto SetIdentity(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for Identity");
  }
  globalConfig.Identity = lua_tostring(state, 1);
  PrintInfo("Identity set to ", globalConfig.Identity);
  return 0;
}

inline auto SetSize(lua_State *state) -> int {
  if (lua_isnumber(state, 1) == 0 || lua_isnumber(state, 2) == 0) {
    return luaL_error(state, "Expected two numbers for Size");
  }
  globalConfig.Size.width = static_cast<int32_t>(lua_tointeger(state, 1));
  globalConfig.Size.height = static_cast<int32_t>(lua_tointeger(state, 2));
  PrintInfo("Size set to {}x{}", globalConfig.Size.width,
            globalConfig.Size.height);
  return 0;
}

inline auto LuaSetLogLevel(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for LogLevel");
  }
  const char *levelStr = lua_tostring(state, 1);
  if (levelStr == nullptr) {
    return luaL_error(state, "Invalid string for LogLevel");
  }

  std::string level(levelStr);
  if (level == "debug") {
    SetLogLevel(LogLevel::Debug);
  } else if (level == "info") {
    SetLogLevel(LogLevel::Info);
  } else if (level == "warning") {
    SetLogLevel(LogLevel::Warning);
  } else if (level == "error") {
    SetLogLevel(LogLevel::Error);
  } else if (level == "fatal") {
    SetLogLevel(LogLevel::Fatal);
  } else {
    return luaL_error(state, "Unknown LogLevel: %s", levelStr);
  }

  PrintInfo("LogLevel set to ", level);
  return 0;
}

inline auto SetFunctions(lua_State *state) -> void {
  lua_pushcfunction(state, SetVsync);
  lua_setfield(state, -2, "_setVsync");

  lua_pushcfunction(state, SetTitle);
  lua_setfield(state, -2, "_setTitle");

  lua_pushcfunction(state, SetIdentity);
  lua_setfield(state, -2, "_setIdentity");

  lua_pushcfunction(state, SetSize);
  lua_setfield(state, -2, "_setSize");

  lua_pushcfunction(state, LuaSetLogLevel);
  lua_setfield(state, -2, "_setLogLevel");
}

inline auto RemoveFunctions(lua_State *state) -> void {
  LuaWrap::SetStackToTable(state, "Thorium"); // [Thorium]

  std::vector<std::string> keysToRemove;

  lua_pushnil(state);                // first key for lua_next
  while (lua_next(state, -2) != 0) { // [Thorium, key, value]
    if (lua_type(state, -2) == LUA_TSTRING) {
      const char *key = lua_tostring(state, -2);
      // NOLINTNEXTLINE
      if (key[0] == '_') {
        keysToRemove.emplace_back(key);
      }
    }
    lua_pop(state, 1); // pop value, keep key
  }

  // Remove collected keys
  for (auto &key : keysToRemove) {
    lua_pushstring(state, key.c_str()); // [Thorium, key]
    lua_pushnil(state);                 // [Thorium, key, nil]
    lua_rawset(state, -3);              // Thorium[key] = nil
  }

  lua_pop(state, 1); // pop Thorium table
}

auto Configure(lua_State *state) -> Result<ApplicationConfig> {

  LuaWrap::SetStackToTable(state, "Thorium"); // [Thorium]

  SetFunctions(state); // [Thorium with set functions]
  lua_pop(state, 1);   // Pop Thorium table

  auto constexpr luaScript = R"lua(
    -- User configuration script
    package.path = package.path .. ";./src/Engine/?.lua"

    -- Make sure the user only edits defined config values
    -- Otherwise it might get confusing why their settings don't apply
    local metatable = {
      __index = function(table, key)
        error("Attempt to read undefined config value: " .. key)
      end,
      __newindex = function(table, key, value)
        error("Attempt to write undefined config value: " .. key)
      end
    }

    local config = {
      window = {
        vsync = false,
        title = "My Awesome Game",
        width = 800,
        height = 600
      },
      filesystem = {
        identity = "MyGame",
      },
      loglevel = ""
    }

    local function recursiveSetMetatable(t)
      setmetatable(t, metatable)
      for k, v in pairs(t) do
        if type(v) == "table" then
          recursiveSetMetatable(v)
        end
      end
    end

    recursiveSetMetatable(config)

    local success, err = pcall(require, "configuration")

    if not success then
      print("No configuration.lua found or error in file: " .. err)
    end

    if (type(Thorium.config) == "function") then
      -- Call user-defined configuration function, not a pcall, this needs to run without errors
      Thorium.config(config)
    end

    Thorium._setVsync(config.window.vsync)
    Thorium._setTitle(config.window.title)
    Thorium._setIdentity(config.filesystem.identity)
    Thorium._setSize(config.window.width, config.window.height)
    if config.loglevel ~= "" then Thorium._setLogLevel(config.loglevel) end
  )lua";

  if (luaL_dostring(state, luaScript) != LUA_OK) {
    const char *errorMsg = lua_tostring(state, -1);
    lua_pop(state, 1); // Pop error message
    return Error::Unexpected(
        std::string("Lua error: ") +
        ((errorMsg != nullptr) ? errorMsg : "Unknown error"));
  }

  // Stack is clean here

  RemoveFunctions(state);

  return globalConfig;
}

} // namespace Config
