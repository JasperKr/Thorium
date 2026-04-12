#pragma once

#include <flecs.h>
#include <memory>
#include <vector>
namespace Engine {

struct Project {
  std::vector<std::unique_ptr<flecs::world>> Scenes;
};

} // namespace Engine