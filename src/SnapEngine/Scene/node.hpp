#pragma once

#include <flecs.h>
namespace Engine {

struct Node {
  auto DrawGUI(flecs::entity entity) const -> void {}
};

} // namespace Engine