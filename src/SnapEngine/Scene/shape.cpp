#include "shape.hpp"
#include "Scene/boundingBox.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include <lua.h>
#include <vector>

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

// scene:createShape(name, lods)
auto LuaShape::Create(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  const char *name = luaL_checkstring(state, 2);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a World object");
  }

  auto shapeEntity = scene->world->entity(name);

  luaL_checktype(state, 3, LUA_TTABLE);
  int lodCount = static_cast<int>(lua_objlen(state, 3));

  for (int i = 1; i <= lodCount; ++i) {
    lua_pushinteger(state, i);
    lua_gettable(state, 3);

    auto *luaLOD = LuaWrap::ObjectFromLua<LuaLevelOfDetail>(state, -1);
    if (luaLOD == nullptr) {
      return luaL_error(state,
                        "Expected a LevelOfDetail object in the LODs table");
    }

    luaLOD->entity.child_of(shapeEntity);
  }
  lua_pop(state, lodCount);

  BoundingBox boundingBox{};

  shapeEntity.children([&](flecs::entity entity) -> void {
    auto boundingBoxRef = entity.get_ref<BoundingBox>();
    if (boundingBoxRef.get() != nullptr) {
      boundingBox.UnionInPlace(*boundingBoxRef.get());
    }
  });

  shapeEntity.add<Shape>();
  shapeEntity.set<BoundingBox>(boundingBox);
  shapeEntity.add<Transform>();

  auto luaShape = LuaShape::FromEntity(shapeEntity);
  LuaWrap::PushObject(state, LuaShape::GetType(), luaShape.get());

  return 1;
}

const LuaWrap::LuaClass ShapeClass = {
    .Name = "Shape",
    .Type = LuaShape::GetType(),
    .Methods =
        {
            {"getName", LuaShape::GetName},
            {"setName", LuaShape::SetName},
            {"getLODs", LuaShape::GetLODs},
        },
};

} // namespace Engine