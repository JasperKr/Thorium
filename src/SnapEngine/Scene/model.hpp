#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Scene/transform.hpp"
#include "Scene/world.hpp"
#include "drawable.hpp"
#include "flecs/flecs.h"
#include "shape.hpp"
#include <flecs/flecs/addons/cpp/world.hpp>
#include <string>
namespace Engine {
const Type modelType = Type("Model");

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern thread_local uint64_t NextNodeUserdataIndex;
extern thread_local uint64_t NextModelUserdataIndex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

struct Model {
  static auto Create() -> auto {
    auto entity = Ecs.entity();
    entity.add<Transform>();
  }
};

} // namespace Engine