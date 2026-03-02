#pragma once

#include "Modules/Engine/scene.hpp"
#include <vector>

namespace Engine {

struct Project {
  std::vector<Scene> scenes;
};

} // namespace Engine