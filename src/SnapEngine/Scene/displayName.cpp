#include "displayName.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <lua.hpp>

namespace Engine {

auto LuaDisplayName::GetName(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaDisplayName>(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *displayName = entity->try_get<DisplayName>();
  if (displayName == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushstring(state, displayName->Name.c_str());
  return 1;
}

auto LuaDisplayName::SetName(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaDisplayName>(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  auto *displayName = entity->try_get_mut<DisplayName>();
  if (displayName == nullptr) {
    entity->set<DisplayName>({luaL_checkstring(state, 2)});
  } else {
    displayName->Name = luaL_checkstring(state, 2);
  }

  return 0;
}

const ::LuaWrap::LuaComponent DisplayNameComponent = {{
    {"getName", LuaDisplayName::GetName},
    {"setName", LuaDisplayName::SetName},
}};

} // namespace Engine
