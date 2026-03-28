#include "levelOfDetail.hpp"
#include "Graphics/mesh.hpp"
#include "Scene/boundingBox.hpp"
#include "Wrap/wrap.hpp"
#include <lua.hpp>

namespace Engine {

auto LuaLevelOfDetail::GetTransitionThreshold(lua_State *state) -> int {
  auto *luaLevelOfDetail = LuaWrap::ObjectFromLua<LuaLevelOfDetail>(state, 1);

  if (luaLevelOfDetail == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = luaLevelOfDetail->entity.get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  lua_pushnumber(state, levelOfDetail->TransitionThreshold);
  return 1;
}
auto LuaLevelOfDetail::SetTransitionThreshold(lua_State *state) -> int {
  auto *luaLevelOfDetail = LuaWrap::ObjectFromLua<LuaLevelOfDetail>(state, 1);

  if (luaLevelOfDetail == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = luaLevelOfDetail->entity.get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  levelOfDetail->TransitionThreshold =
      static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  return 0;
}

auto LuaLevelOfDetail::GetBoundingBoxes(lua_State *state) -> int {
  auto *luaLevelOfDetail = LuaWrap::ObjectFromLua<LuaLevelOfDetail>(state, 1);

  if (luaLevelOfDetail == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = luaLevelOfDetail->entity.get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  if (lua_gettop(state) < 3) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 3, LUA_TTABLE);
  }

  int index = 1;
  levelOfDetail.entity().children<Graphics::Mesh>(
      [&](const flecs::entity &child) -> void {
        child.children<BoundingBox>(
            [&](const flecs::entity &bboxEntity) -> void {
              lua_pushinteger(state, index++);
              auto bboxObject = LuaBoundingBox(bboxEntity);
              LuaWrap::PushObject(state, LuaBoundingBox::GetType(),
                                  &bboxObject);
              lua_settable(state, -3);
            });
      });

  return 1;
}
auto LuaLevelOfDetail::GetMeshes(lua_State *state) -> int {
  auto *luaLevelOfDetail = LuaWrap::ObjectFromLua<LuaLevelOfDetail>(state, 1);

  if (luaLevelOfDetail == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = luaLevelOfDetail->entity.get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  if (lua_gettop(state) < 2) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 2, LUA_TTABLE);
  }

  int index = 1;
  levelOfDetail.entity().children<Graphics::Mesh>(
      [&](const flecs::entity &child) -> void {
        lua_pushinteger(state, index++);
        auto mesh = child.get_ref<Graphics::Mesh>();
        LuaWrap::PushObject(state, Graphics::Mesh::GetType(), mesh.get());
        lua_settable(state, -3);
      });

  return 1;
}

const LuaWrap::LuaModule LevelOfDetailModule = {
    .Name = "LevelOfDetail",
    .Functions =
        {
            {"GetTransitionThreshold",
             LuaLevelOfDetail::GetTransitionThreshold},
            {"SetTransitionThreshold",
             LuaLevelOfDetail::SetTransitionThreshold},
            {"GetBoundingBoxes", LuaLevelOfDetail::GetBoundingBoxes},
            {"GetMeshes", LuaLevelOfDetail::GetMeshes},
        },
};

} // namespace Engine