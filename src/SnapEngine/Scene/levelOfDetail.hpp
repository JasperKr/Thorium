#pragma once

#include "Modules/Math/mathTypes.hpp"
#include "Scene/boundingBox.hpp"
#include <flecs.h>
#include <string>
#include <vector>
namespace Engine {

struct LevelOfDetail {
  // std::vector<Ref<Graphics::Mesh>> Meshes;
  // std::vector<BoundingBox> BoundingBoxes;
  // BoundingBox CombinedBoundingBox;

  // Threshold is calculated based on the area of the bounding box of the mesh on the screen.
  // Starting at 1.0 and decreasing as the mesh gets smaller on the screen.
  // This threshold determines when to switch to the next level of detail.
  Math::Scalar TransitionThreshold = 0.0F;

  static auto CreateLevelOfDetail(flecs::world &world, const std::string &name,
                                  const std::vector<flecs::entity> &meshes,
                                  const std::vector<BoundingBox> &boundingBoxes,
                                  Math::Scalar transitionThreshold)
      -> flecs::entity {
    auto lod = flecs::entity(world, name.c_str());
    lod.add<LevelOfDetail>(
        LevelOfDetail{.TransitionThreshold = transitionThreshold});

    for (const auto &mesh : meshes) {
      mesh.child_of(lod);
    }

    BoundingBox combinedBoundingBox{};

    for (const auto &boundingBox : boundingBoxes) {
      combinedBoundingBox.UnionInPlace(boundingBox);
      auto bboxEntity = flecs::entity(world).add<BoundingBox>(boundingBox);
      bboxEntity.child_of(lod);
    }

    auto combinedBBoxEntity =
        flecs::entity(world).add<BoundingBox>(combinedBoundingBox);
    combinedBBoxEntity.child_of(lod);

    return lod;
  };
};

} // namespace Engine