#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/Engine/material.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
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

using SceneObject =
    std::variant<Ref<struct Node>, Ref<struct Shape>, Ref<struct Model>>;

struct Selectable {
  std::string Name;
  uint32_t ID;
  void *Userdata;
};

const Type nodeType = Type("Node");
const Type shapeType = Type("Shape");
const Type modelType = Type("Model");

struct Node : Selectable, Object {
  Transform Transform;
  std::vector<SceneObject> Children;

  static auto GetType() -> Type const * { return &nodeType; }
  auto GetInstanceType() const -> const Type * override { return &nodeType; }
};

struct Shape : Selectable, Object {
  Transform Transform;
  Ref<Graphics::Mesh> Mesh;
  Renderer::Material Material;

  static auto GetType() -> Type const * { return &shapeType; }
  auto GetInstanceType() const -> const Type * override { return &shapeType; }
};

struct Model : Selectable, Object {
  Transform Transform;
  std::vector<Shape> Shapes;

  static auto GetType() -> Type const * { return &modelType; }
  auto GetInstanceType() const -> const Type * override { return &modelType; }
};

} // namespace Engine