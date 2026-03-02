#pragma once

#include "Modules/Engine/model.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Engine {

const Type sceneType = Type("Scene");

struct Scene : Object {
  std::vector<SceneObject> hierarchy;

  auto GetSceneObject(lua_State *state) -> int;

  static auto LoadBinding(lua_State *state) -> int;
  static auto GetHierarchyObjects(lua_State *state) -> int;
  static auto AddHierarchyObject(lua_State *state) -> int;
  static auto GetHierarchyObject(lua_State *state) -> int;
  static auto RemoveHierarchyObject(lua_State *state) -> int;

  static auto GetType() -> Type const * { return &sceneType; }
  auto GetInstanceType() const -> const Type * override { return &sceneType; }
};

} // namespace Engine