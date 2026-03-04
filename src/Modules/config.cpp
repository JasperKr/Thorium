#include "config.hpp"
#include "Graphics/deviceSettings.hpp"
#include "Modules/console.hpp"
#include "Wrap/Modules/wrap_window.hpp"
#include "Wrap/wrap.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Config {

// NOLINTNEXTLINE
ApplicationConfig globalConfig;

inline auto SetIdentity(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for Identity");
  }
  globalConfig.Identity = lua_tostring(state, 1);
  PrintInfo("Identity set to ", globalConfig.Identity);
  return 0;
}

inline auto StringToExtensionRequirement(const char *str)
    -> Result<Graphics::ExtensionRequirement> {
  if (std::string(str) == "disabled") {
    return Graphics::ExtensionRequirement::Disabled;
  }
  if (std::string(str) == "optional") {
    return Graphics::ExtensionRequirement::Optional;
  }
  if (std::string(str) == "required") {
    return Graphics::ExtensionRequirement::Required;
  }

  return Error::Unexpectedf("Invalid extension requirement '{}'", str);
}

inline auto SetGraphicsSettings(lua_State *state) -> int {
  lua_getfield(state, 1, "hardwareRaytracing");
  if (!lua_isnoneornil(state, -1)) {
    auto extRequirementResult =
        StringToExtensionRequirement(luaL_checkstring(state, -1));
    if (Error::IsError(extRequirementResult)) {
      return luaL_error(state, "%s",
                        extRequirementResult.error().message.c_str());
    }
    globalConfig.deviceSettings.hardwareRaytracing =
        extRequirementResult.value();
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "inlineRaytracing");
  if (!lua_isnoneornil(state, -1)) {
    auto extRequirementResult =
        StringToExtensionRequirement(luaL_checkstring(state, -1));
    if (Error::IsError(extRequirementResult)) {
      return luaL_error(state, "%s",
                        extRequirementResult.error().message.c_str());
    }
    globalConfig.deviceSettings.inlineRaytracing = extRequirementResult.value();
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "requiredExtensions");
  if (!lua_isnoneornil(state, -1)) {
    if (!lua_istable(state, -1)) {
      return luaL_error(state, "requiredExtensions must be a table");
    }

    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
      if (lua_type(state, -2) != LUA_TSTRING ||
          lua_type(state, -1) != LUA_TSTRING) {
        return luaL_error(state, "requiredExtensions must be a table "
                                 "with string keys and values");
      }
      const char *extName = lua_tostring(state, -2);
      const char *extReq = lua_tostring(state, -1);
      Graphics::ExtensionRequirement extRequirement{};

      auto extRequirementResult = StringToExtensionRequirement(extReq);
      if (Error::IsError(extRequirementResult)) {
        return luaL_error(state, "%s",
                          extRequirementResult.error().message.c_str());
      }

      extRequirement = extRequirementResult.value();
      globalConfig.deviceSettings.requiredExtensions.emplace_back(
          Graphics::Extension{
              .name = extName,
              // Instance-level extensions are not supported by lua configuration.
              .type = Graphics::ExtensionType::Device,
              .requirement = extRequirement,
          });
    }
  }
  lua_pop(state, 1);

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

  PrintInfo("LogLevel set to {}", levelStr);
  return 0;
}

inline auto SetFunctions(lua_State *state) -> void {
  lua_pushcfunction(state, SetIdentity);
  lua_setfield(state, -2, "_setIdentity");

  lua_pushcfunction(state, LuaSetLogLevel);
  lua_setfield(state, -2, "_setLogLevel");

  lua_pushcfunction(state, Wrap::Window::wrap_SetInitialSettings);
  lua_setfield(state, -2, "_setSettings");

  lua_pushcfunction(state, SetGraphicsSettings);
  lua_setfield(state, -2, "_setGraphicsSettings");
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

    local config = {
      filesystem = {
        identity = "MyGame",
      },
      window = {},
      graphics = {
        hardwareRaytracing = "disabled",
        inlineRaytracing = "disabled",
      },
      loglevel = ""
    }

    local success, err = pcall(require, "configuration")

    if not success then
      print("No configuration.lua found or error in file: " .. err)
    end

    if (type(Thorium.config) == "function") then
      -- Call user-defined configuration function, not a pcall, this needs to run without errors
      Thorium.config(config)
    end

    Thorium._setIdentity(config.filesystem.identity)
    Thorium._setSettings(config.window)
    Thorium._setGraphicsSettings(config.graphics or {})
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
