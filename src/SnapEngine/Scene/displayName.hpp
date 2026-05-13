#pragma once

#include "Wrap/wrap.hpp"
#include <lua.h>
#include <string>
namespace Engine {

struct DisplayName {
  std::string Name;

  auto DrawGUI(lua_State *state) const -> void;

  static auto GetName(lua_State *state) -> int;
  static auto SetName(lua_State *state) -> int;
};

extern const LuaWrap::LuaComponent DisplayNameComponent;

} // namespace Engine