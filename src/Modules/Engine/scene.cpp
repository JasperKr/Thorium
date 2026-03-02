#include "scene.hpp"
#include "Modules/Engine/model.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include <variant>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Engine {

auto Scene::LoadBinding(lua_State *state) -> int {
  // auto binding = Bindings::LuaBoundStruct<Scene>("Scene");
  // binding.Register(state);

  return 1;
}

auto Scene::GetHierarchyObjects(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  if (lua_gettop(state) == 1) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 2, LUA_TTABLE);
  }

  auto index = 1;

  for (const auto &hierarchyObject : scene->hierarchy) {
    if (std::holds_alternative<Ref<Node>>(hierarchyObject)) {
      const auto &object = std::get<Ref<Node>>(hierarchyObject);
      LuaWrap::PushObject(state, Node::GetType(), object.get());
    } else if (std::holds_alternative<Ref<Shape>>(hierarchyObject)) {
      const auto &object = std::get<Ref<Shape>>(hierarchyObject);
      LuaWrap::PushObject(state, Shape::GetType(), object.get());
    } else if (std::holds_alternative<Ref<Model>>(hierarchyObject)) {
      const auto &object = std::get<Ref<Model>>(hierarchyObject);
      LuaWrap::PushObject(state, Model::GetType(), object.get());
    } else {
      luaL_error(state, "Unimplemented scene object type");
    }

    lua_rawseti(state, -2, index);
  }

  return 1;
}
auto Scene::AddHierarchyObject(lua_State *state) -> int {}
auto Scene::GetHierarchyObject(lua_State *state) -> int {}
auto Scene::RemoveHierarchyObject(lua_State *state) -> int {}
} // namespace Engine