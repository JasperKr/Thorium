#include "config.hpp"
#include <iostream>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

/*
void SetVsync(bool vsync);
  auto GetVsync() const -> bool;
  void SetTitle(const std::string &title);
  auto GetTitle() const -> std::string;
  void SetIdentity(const std::string &identity);
  auto GetIdentity() const -> std::string;
  void SetSize(uint32_t width, uint32_t height);
  auto GetSize() const -> WindowSize;
*/

namespace Config {

// NOLINTNEXTLINE
static ApplicationConfig globalConfig;

inline auto SetVsync(lua_State *state) -> int {
  if (!lua_isboolean(state, 1)) {
    return luaL_error(state, "Expected boolean for Vsync");
  }
  globalConfig.Vsync = (lua_toboolean(state, 1) != 0);
  std::cout << "Vsync set to " << (globalConfig.Vsync ? "true" : "false")
            << "\n";
  return 0;
}

inline auto SetTitle(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for Title");
  }
  globalConfig.Title = lua_tostring(state, 1);
  std::cout << "Title set to " << globalConfig.Title << "\n";
  return 0;
}

inline auto SetIdentity(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for Identity");
  }
  globalConfig.Identity = lua_tostring(state, 1);
  std::cout << "Identity set to " << globalConfig.Identity << "\n";
  return 0;
}

inline auto SetSize(lua_State *state) -> int {
  if (lua_isnumber(state, 1) == 0 || lua_isnumber(state, 2) == 0) {
    return luaL_error(state, "Expected two numbers for Size");
  }
  globalConfig.Size.width = static_cast<int32_t>(lua_tointeger(state, 1));
  globalConfig.Size.height = static_cast<int32_t>(lua_tointeger(state, 2));
  std::cout << "Size set to " << globalConfig.Size.width << "x"
            << globalConfig.Size.height << "\n";
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
}

inline auto RemoveFunctions(lua_State *state) -> void {
  lua_getglobal(state, "Thorium"); // push Thorium table
  lua_pushnil(state);              // first key for lua_next

  while (lua_next(state, -2) != 0) {
    // key at -2, value at -1
    if (lua_type(state, -2) == LUA_TSTRING) {
      const char *key = lua_tostring(state, -2);
      if (key[0] == '_') { // NOLINT, pointer arithmatic is safe here, since lua
                           // strings are null-terminated
        lua_pop(state, 1); // pop value
        lua_pushstring(state, key);
        lua_pushnil(state);
        lua_settable(state, -3); // Thorium[key] = nil
        continue;                // skip lua_pop below
      }
    }
    lua_pop(state, 1); // pop value, keep key for lua_next
  }

  lua_pop(state, 1); // pop Thorium table
}

auto Configure(lua_State *state)
    -> tl::expected<ApplicationConfig, Error::Error> {

  lua_newtable(state);
  lua_setglobal(state, "Thorium");

  lua_getglobal(state, "Thorium");
  SetFunctions(state);
  lua_pop(state, 1); // Pop Thorium table

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
      }
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

    if (type(Configuration) == "function") then
      -- Call user-defined configuration function, not a pcall, this needs to run without errors
      Configuration(config)
    end

    Thorium._setVsync(config.window.vsync)
    Thorium._setTitle(config.window.title)
    Thorium._setIdentity(config.filesystem.identity)
    Thorium._setSize(config.window.width, config.window.height)
  )lua";

  if (luaL_dostring(state, luaScript) != LUA_OK) {
    const char *errorMsg = lua_tostring(state, -1);
    lua_pop(state, 1); // Pop error message
    return tl::unexpected(Error::Error{
        .message = std::string("Lua error: ") +
                   ((errorMsg != nullptr) ? errorMsg : "Unknown error")});
  }

  return globalConfig;
}

} // namespace Config
