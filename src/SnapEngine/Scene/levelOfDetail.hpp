#pragma once

#include "Modules/Math/mathTypes.hpp"
#include "Scene/boundingBox.hpp"
#include <flecs.h>
#include <string>
namespace Engine {

struct LevelOfDetail {
  // Threshold is calculated based on the area of the bounding box of the mesh on the screen.
  // Starting at 1.0 and decreasing as the mesh gets smaller on the screen.
  // This threshold determines when to switch to the next level of detail.
  Math::Scalar TransitionThreshold = 0.0F;

  static auto CreateLevelOfDetail(flecs::world &world, const std::string &name,
                                  Math::Scalar transitionThreshold)
      -> flecs::entity {
    auto lod = world.entity(name.c_str());
    lod.set<LevelOfDetail>(
        LevelOfDetail{.TransitionThreshold = transitionThreshold});

    lod.add<BoundingBox>();

    return lod;
  };
};

static const Type levelOfDetailType = Type("LevelOfDetail");

struct LuaLevelOfDetail : Object {
  flecs::entity entity;

  explicit LuaLevelOfDetail(const flecs::entity &entity) : entity(entity) {}

  static auto GetType() -> const Type * { return &levelOfDetailType; }
  auto GetInstanceType() const -> const Type * override {
    return &levelOfDetailType;
  }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaLevelOfDetail> {
    return Ref<LuaLevelOfDetail>::Make(entity);
  }

  static auto Create(lua_State *state) -> int;

  static auto GetTransitionThreshold(lua_State *state) -> int;
  static auto SetTransitionThreshold(lua_State *state) -> int;

  static auto AddGeometry(lua_State *state) -> int;
  static auto RemoveGeometry(lua_State *state) -> int;

  static auto GetMeshes(lua_State *state) -> int;
  static auto GetBoundingBox(lua_State *state) -> int;
};

extern const LuaWrap::LuaClass LevelOfDetailClass;

} // namespace Engine