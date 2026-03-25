#include "selectable.hpp"
#include "Wrap/wrap.hpp"
#include "entity.hpp"
#include <lua.h>

namespace Engine {
auto Selectable::SetUserdata(lua_State *state) -> int {
  // Stack: [entity, userdata]

  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 2) {
    return luaL_error(state, "Expected 1 argument for userdata");
  }

  auto *selectable = entity->try_get_mut<Selectable>();
  if (selectable == nullptr) {
    entity->set<Selectable>({});
    selectable = entity->try_get_mut<Selectable>();
  }

  if (selectable->userdataIndex == 0) {
    selectable->userdataIndex = GetFreeUserdataIndex();
  }

  LuaWrap::SetStackToRegistry(state, "SelectableUserdata");
  // Stack: [entity, userdata, "SelectableUserdata"]
  lua_pushvalue(state, 2);
  // Stack: [entity, userdata, "SelectableUserdata", userdata]

  lua_rawseti(state, -2, static_cast<int>(selectable->userdataIndex));
  // raw set integer -> SelectableUserdata[selectable->userdataIndex] = userdata
  // Stack: [entity, userdata, "SelectableUserdata"]
  return 0;
}

auto Selectable::GetUserdata(lua_State *state) -> int {
  // Stack: [entity]

  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *selectable = entity->try_get<Selectable>();
  if (selectable == nullptr || selectable->userdataIndex == 0) {
    lua_pushnil(state);
    return 1;
  }

  LuaWrap::SetStackToRegistry(state, "SelectableUserdata");
  // Stack: [entity, "SelectableUserdata"]
  lua_rawgeti(state, -1, static_cast<int>(selectable->userdataIndex));
  // Stack: [entity, "SelectableUserdata", userdata]
  return 1;
}
} // namespace Engine