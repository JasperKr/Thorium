#pragma once

#include "Scene/levelOfDetail.hpp"
#include <vector>
namespace Engine {

struct Shape {
  static auto CreateShape(flecs::world &world, const std::string &name,
                          const std::vector<LevelOfDetail> &lods)
      -> flecs::entity {
    auto shapeEntity = flecs::entity(world, name.c_str());
    shapeEntity.add<Shape>();

    for (const auto &lod : lods) {
      auto lodEntity = flecs::entity(world).add<LevelOfDetail>(lod);
      lodEntity.child_of(shapeEntity);
    }

    return shapeEntity;
  }
};

}; // namespace Engine