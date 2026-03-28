#pragma once

#include "Modules/Math/mathTypes.hpp"
#include "Scene/boundingBox.hpp"
#include <flecs.h>
#include <string>
#include <vector>
namespace Engine {

struct LevelOfDetail {
  // Threshold is calculated based on the area of the bounding box of the mesh on the screen.
  // Starting at 1.0 and decreasing as the mesh gets smaller on the screen.
  // This threshold determines when to switch to the next level of detail.
  Math::Scalar TransitionThreshold = 0.0F;

  static auto CreateLevelOfDetail(flecs::world &world, const std::string &name,
                                  const std::vector<flecs::entity> &meshes,
                                  Math::Scalar transitionThreshold)
      -> flecs::entity {
    auto lod = world.entity(name.c_str());
    lod.set<LevelOfDetail>(
        LevelOfDetail{.TransitionThreshold = transitionThreshold});

    for (const auto &mesh : meshes) {
      mesh.child_of(lod);
    }

    BoundingBox combinedBoundingBox{};

    auto combinedBBoxEntity =
        flecs::entity(world).set<BoundingBox>(combinedBoundingBox);
    combinedBBoxEntity.child_of(lod);

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

  static auto GetTransitionThreshold(lua_State *state) -> int;
  static auto SetTransitionThreshold(lua_State *state) -> int;

  static auto GetBoundingBoxes(lua_State *state) -> int;
  static auto GetMeshes(lua_State *state) -> int;
};

extern const LuaWrap::LuaClass LevelOfDetailClass;

} // namespace Engine