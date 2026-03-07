#pragma once

#include "Modules/error.hpp"
#include <unordered_map>
#include <vector>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

// Enum wrapper

namespace LuaWrap {

inline auto EnumFromLua(lua_State *state, int index,
                        std::unordered_map<std::string, int> enumMap)
    -> Result<int> {
  const char *str = luaL_checkstring(state, index);
  auto iter = enumMap.find(str);
  if (iter == enumMap.end()) {
    return Error::Unexpected("Invalid enum value: " + std::string(str));
  }
  return iter->second;
}

inline auto EnumToLua(lua_State *state, int value,
                      const std::unordered_map<std::string, int> &enumMap)
    -> Error {
  for (const auto &[key, val] : enumMap) {
    if (val == value) {
      lua_pushstring(state, key.c_str());
      return Error::Success();
    }
  }

  return Error::Createf("Invalid enum value: {}", value);
}

inline auto
EnumToLua(lua_State *state, int value,
          const std::unordered_map<int, std::string> &reverseEnumMap) -> Error {
  auto iter = reverseEnumMap.find(value);
  if (iter == reverseEnumMap.end()) {
    return Error::Createf("Invalid enum value: {}", value);
  }

  lua_pushstring(state, iter->second.c_str());
  return Error::Success();
}

template <typename T> struct LuaEnum {
private:
  std::unordered_map<std::string, int> EnumMap;
  std::unordered_map<int, std::string> ReverseEnumMap;

public:
  explicit LuaEnum(const std::vector<std::pair<std::string, int>> &entries) {
    for (const auto &[key, value] : entries) {
      EnumMap[key] = value;
      ReverseEnumMap[value] = key;
    }
  }

  auto FromLua(lua_State *state, int index) -> Result<T> {
    auto result = EnumFromLua(state, index, EnumMap);
    if (Error::IsError(result)) {
      return result.error().AsUnexpected();
    }
    return static_cast<T>(result.value());
  }

  auto ToLua(lua_State *state, T value) -> Error {
    return EnumToLua(state, static_cast<int>(value), ReverseEnumMap);
  }
};

} // namespace LuaWrap