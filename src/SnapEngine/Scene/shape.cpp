#include "shape.hpp"
#include "Wrap/wrap.hpp"

namespace Engine {

auto LuaShape::GetName(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<LuaShape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Expected a Shape object");
  }

  lua_pushstring(state, shape->entity.name());
  return 1;
}

auto LuaShape::SetName(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<LuaShape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Expected a Shape object");
  }

  const char *newName = luaL_checkstring(state, 2);
  shape->entity.set_name(newName);
  return 0;
}

auto LuaShape::GetLODs(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<LuaShape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Expected a Shape object");
  }

  if (lua_gettop(state) == 1) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 2, LUA_TTABLE);
  }

  int index = 1;
  shape->entity.children([&](flecs::entity entity) -> void {
    lua_pushinteger(state, index++);
    auto lodObject = LuaLevelOfDetail::FromEntity(entity);
    LuaWrap::PushObject(state, LuaLevelOfDetail::GetType(), lodObject.get());
    lua_settable(state, -3);
  });

  return 1;
}

const LuaWrap::LuaModule ShapeModule = {
    .Name = "Shape",
    .Functions =
        {
            {"GetName", LuaShape::GetName},
            {"SetName", LuaShape::SetName},
            {"GetLODs", LuaShape::GetLODs},
        },
};

} // namespace Engine