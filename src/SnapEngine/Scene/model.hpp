#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include <flecs.h>
#include <string>
#include <vector>
namespace Engine {

struct Model {
  static auto CreateModel(flecs::world &world, const std::string &name,
                          const std::vector<flecs::entity> &shapes,
                          const flecs::entity &material) -> flecs::entity {
    auto modelEntity = flecs::entity(world, name.c_str());
    modelEntity.add<Model>();

    modelEntity.add<Userdata>();
    modelEntity.add<Transform>();

    for (const auto &shape : shapes) {
      shape.child_of(modelEntity);
    }

    material.child_of(modelEntity);
    return modelEntity;
  }
};

static const Type modelType = Type("Model");

struct LuaModel : Object {
  explicit LuaModel(const flecs::entity &entity) : entity(entity) {}

  flecs::entity entity;

  static auto GetType() -> const Type * { return &modelType; }
  auto GetInstanceType() const -> const Type * override { return &modelType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaModel> {
    return Ref<LuaModel>::Make(entity);
  }

  static auto Create(lua_State *state) -> int;

  static auto GetName(lua_State *state) -> int;
  static auto SetName(lua_State *state) -> int;

  static auto GetPosition(lua_State *state) -> int;
  static auto SetPosition(lua_State *state) -> int;

  static auto GetRotation(lua_State *state) -> int;
  static auto SetRotation(lua_State *state) -> int;

  static auto GetScale(lua_State *state) -> int;
  static auto SetScale(lua_State *state) -> int;

  static auto GetShapes(lua_State *state) -> int;
};

extern const LuaWrap::LuaModule ModelModule;

} // namespace Engine