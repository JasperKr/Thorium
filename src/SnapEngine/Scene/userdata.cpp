#include "userdata.hpp"
#include "Wrap/wrap.hpp"
#include "entity.hpp"
#include <lua.h>

namespace Engine {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local std::vector<int32_t> FreedUserdataIndices{};

auto Userdata::SetUserdata(lua_State *state) -> int {
  // Stack: [entity, userdata]

  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 2) {
    return luaL_error(state, "Expected 1 argument for userdata");
  }

  auto *userdata = entity->try_get_mut<Userdata>();
  if (userdata == nullptr) {
    entity->set<Userdata>({});
    userdata = entity->try_get_mut<Userdata>();
  }

  if (userdata->userdataIndex == 0) {
    userdata->userdataIndex = GetFreeUserdataIndex();
  }

  LuaWrap::SetStackToRegistry(state, "Userdata");
  // Stack: [entity, userdata, "Userdata"]
  lua_pushvalue(state, 2);
  // Stack: [entity, userdata, "Userdata", userdata]

  lua_rawseti(state, -2, static_cast<int>(userdata->userdataIndex));
  // raw set integer -> Userdata[userdata->userdataIndex] = userdata
  // Stack: [entity, userdata, "Userdata"]
  return 0;
}

auto Userdata::GetUserdata(lua_State *state) -> int {
  // Stack: [entity]

  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *userdata = entity->try_get<Userdata>();
  if (userdata == nullptr || userdata->userdataIndex == 0) {
    lua_pushnil(state);
    return 1;
  }

  LuaWrap::SetStackToRegistry(state, "Userdata");
  // Stack: [entity, "Userdata"]
  lua_rawgeti(state, -1, static_cast<int>(userdata->userdataIndex));
  // Stack: [entity, "Userdata", userdata]
  return 1;
}
} // namespace Engine