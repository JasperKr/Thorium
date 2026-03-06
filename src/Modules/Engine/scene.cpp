#include "scene.hpp"
#include "Modules/Engine/model.hpp"
#include "Modules/object.hpp"
#include "Wrap/proxy.hpp"
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
    auto error = LuaWrap::PushVariant(state, hierarchyObject);
    if (Error::IsError(error)) {
      return luaL_error(state, "Failed to push scene object: %s",
                        error.message.c_str());
    }

    lua_rawseti(state, -2, index);
  }

  return 1;
}
auto Scene::AddHierarchyObject(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  auto result = LuaWrap::VariantFromLua<SceneObject>(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to parse scene object: %s",
                      result.error().message.c_str());
  }

  scene->hierarchy.emplace_back(result.value());

  return 1;
}
auto Scene::GetHierarchyObject(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
  if (index < 1 || index > scene->hierarchy.size()) {
    return luaL_error(state, "Index out of bounds");
  }

  const auto &hierarchyObject = scene->hierarchy[index - 1];

  auto error = LuaWrap::PushVariant(state, hierarchyObject);
  if (Error::IsError(error)) {
    return luaL_error(state, "Failed to push scene object: %s",
                      error.message.c_str());
  }

  return 1;
}
auto Scene::RemoveHierarchyObject(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
  if (index < 1 || index > scene->hierarchy.size()) {
    return luaL_error(state, "Index out of bounds");
  }

  scene->hierarchy.erase(scene->hierarchy.begin() + (index - 1));

  return 0;
}
} // namespace Engine