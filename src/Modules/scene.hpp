#pragma once

#include "Modules/model.hpp"
#include <vector>

namespace Engine {

struct Scene {
  std::vector<SceneObject> hierarchy;
};

} // namespace Engine