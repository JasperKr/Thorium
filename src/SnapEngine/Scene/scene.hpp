#pragma once
#include "Modules/console.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Scene/boundingBox.hpp"
#include "Scene/geometry.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Scene/model.hpp"
#include "Scene/node.hpp"
#include "Scene/shape.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "flecs.h"
#include <lua.hpp>
#include <string>
#include <utility>

namespace Engine {

static const Type SceneType = Type("Scene");

struct Scene : Object {
  flecs::world world;
  std::string name;

  explicit Scene(std::string name) : name(std::move(name)) {
    world.component<Geometry>();
    world.component<BoundingBox>();
    world.component<Transform>();
    world.component<LevelOfDetail>();
    world.component<Model>();
    world.component<Node>();
    world.component<Shape>();
    world.component<Userdata>();
  }

  static auto GetType() -> const Type * { return &SceneType; }
  auto GetInstanceType() const -> const Type * override { return &SceneType; }

  static auto LoadBinding(lua_State *state) -> int;
  static auto DrawUiElement(lua_State *state) -> int;
  static auto DrawModels(lua_State *state) -> int;
};

extern const LuaWrap::LuaClass SceneLuaClass;

} // namespace Engine