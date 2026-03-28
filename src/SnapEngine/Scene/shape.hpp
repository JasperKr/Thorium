#pragma once

#include "Modules/object.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Wrap/wrap.hpp"
#include <flecs.h>
#include <lua.hpp>
#include <vector>
namespace Engine {

struct Shape {
  static auto Create(flecs::world &world, const std::string &name,
                     const std::vector<LevelOfDetail> &lods) -> flecs::entity {
    auto shapeEntity = flecs::entity(world, name.c_str());
    shapeEntity.add<Shape>();

    for (const auto &lod : lods) {
      auto lodEntity = flecs::entity(world).set<LevelOfDetail>(lod);
      lodEntity.child_of(shapeEntity);
    }

    return shapeEntity;
  }
};

static const Type shapeType = Type("Shape");

struct LuaShape : Object {
  explicit LuaShape(const flecs::entity &entity) : entity(entity) {}

  flecs::entity entity;

  static auto GetType() -> const Type * { return &shapeType; }
  auto GetInstanceType() const -> const Type * override { return &shapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaShape> {
    return Ref<LuaShape>::Make(entity);
  }

  static auto GetName(lua_State *state) -> int;
  static auto SetName(lua_State *state) -> int;
  static auto GetLODs(lua_State *state) -> int;
};

extern const LuaWrap::LuaClass ShapeClass;

}; // namespace Engine