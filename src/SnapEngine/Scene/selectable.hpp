#pragma once

#include "identifier.hpp"
#include "lua.hpp"
#include <cstdint>
#include <string>

namespace Engine {
struct Selectable {
  std::string name;
  Identifier id = GenerateIdentifier();
  uint64_t userdataIndex = 0;

  static auto SetUserdata(lua_State *state, uint64_t &userdataIndex) -> int;
  static auto GetUserdata(lua_State *state, uint64_t &userdataIndex) -> int;
};

} // namespace Engine