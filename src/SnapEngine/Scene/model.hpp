#pragma once

#include "Scene/selectable.hpp"
#include "Scene/transform.hpp"
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

    modelEntity.add<Selectable>(Selectable{.name = name});
    modelEntity.add<Transform>();

    for (const auto &shape : shapes) {
      shape.child_of(modelEntity);
    }

    material.child_of(modelEntity);
    return modelEntity;
  }
};

} // namespace Engine