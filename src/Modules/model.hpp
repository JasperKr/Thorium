#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/material.hpp"
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
namespace Engine {
struct Transform {
  Math::Vec3 Position{};
  Math::Quaternion Rotation;
  Math::Vec3 Scale{1.0F, 1.0F, 1.0F};
};

using SceneObject = std::variant<struct Node, struct Shape, struct Model>;

struct Selectable {
  std::string Name;
  uint32_t ID;
  void *Userdata;
};

struct Node : Selectable {
  Transform Transform;
  std::vector<SceneObject> Children;
};

struct Shape : Selectable {
  Transform Transform;
  Ref<Graphics::Mesh> Mesh;
  Renderer::Material Material;
};

struct Model : Selectable {
  Transform Transform;
  std::vector<Shape> Shapes;
};

struct Scene {
  std::vector<SceneObject> Nodes;
};

} // namespace Engine