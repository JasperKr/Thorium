#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "drawable.hpp"
#include "identifier.hpp"
#include "material.hpp"
#include <string>
#include <variant>
#include <vector>
namespace Engine {
const Type nodeType = Type("Node");
const Type shapeType = Type("Shape");
const Type modelType = Type("Model");

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern thread_local uint64_t NextNodeUserdataIndex;
extern thread_local uint64_t NextModelUserdataIndex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

struct Transform {
  Math::Vec3 Position{};
  Math::Quaternion Rotation;
  Math::Vec3 Scale{1.0F, 1.0F, 1.0F};

  auto PushPosition(lua_State *state) const -> void;
  auto ReadPosition(lua_State *state) -> void;

  auto PushRotation(lua_State *state) const -> void;
  auto ReadRotation(lua_State *state) -> void;

  auto PushScale(lua_State *state) const -> void;
  auto ReadScale(lua_State *state) -> void;
};

using SceneObject =
    std::variant<Ref<struct Node>, Ref<struct Shape>, Ref<struct Model>>;

struct Selectable {
  std::string name;
  Identifier id = GenerateIdentifier();
  uint64_t userdataIndex = 0;

  static auto SetUserdata(lua_State *state, uint64_t &userdataIndex) -> int;
  static auto GetUserdata(lua_State *state, uint64_t &userdataIndex) -> int;
};

struct Node : Selectable, Object, UiElement {
  Transform transform;
  std::vector<SceneObject> children;

  static auto GetType() -> Type const * { return &nodeType; }
  auto GetInstanceType() const -> const Type * override { return &nodeType; }
  auto DrawUiElement() -> Error override;

  static auto wrap_SetPosition(lua_State *state) -> int;
  static auto wrap_GetPosition(lua_State *state) -> int;

  static auto wrap_SetRotation(lua_State *state) -> int;
  static auto wrap_GetRotation(lua_State *state) -> int;

  static auto wrap_SetScale(lua_State *state) -> int;
  static auto wrap_GetScale(lua_State *state) -> int;

  static auto wrap_AddChild(lua_State *state) -> int;
  static auto wrap_GetChild(lua_State *state) -> int;
  static auto wrap_RemoveChild(lua_State *state) -> int;
  static auto wrap_GetChildren(lua_State *state) -> int;

  static auto wrap_SetName(lua_State *state) -> int;
  static auto wrap_GetName(lua_State *state) -> int;

  static auto wrap_SetUserdata(lua_State *state) -> int;
  static auto wrap_GetUserdata(lua_State *state) -> int;

  static auto LoadBinding(lua_State *state) -> int;
};

struct Shape : Selectable, Object, UiElement {
  Transform transform;
  Ref<Graphics::Mesh> mesh;
  Ref<Renderer::Material> material;

  static auto GetType() -> Type const * { return &shapeType; }
  auto GetInstanceType() const -> const Type * override { return &shapeType; }
  auto DrawUiElement() -> Error override;

  static auto wrap_SetPosition(lua_State *state) -> int;
  static auto wrap_GetPosition(lua_State *state) -> int;

  static auto wrap_SetRotation(lua_State *state) -> int;
  static auto wrap_GetRotation(lua_State *state) -> int;

  static auto wrap_SetScale(lua_State *state) -> int;
  static auto wrap_GetScale(lua_State *state) -> int;

  static auto wrap_SetMesh(lua_State *state) -> int;
  static auto wrap_GetMesh(lua_State *state) -> int;

  static auto wrap_SetMaterial(lua_State *state) -> int;
  static auto wrap_GetMaterial(lua_State *state) -> int;

  static auto wrap_SetUserdata(lua_State *state) -> int;
  static auto wrap_GetUserdata(lua_State *state) -> int;

  static auto LoadBinding(lua_State *state) -> int;
};

struct Model : Selectable, Object, UiElement {
  Transform transform;
  std::vector<Ref<Shape>> shapes;

  static auto GetType() -> Type const * { return &modelType; }
  auto GetInstanceType() const -> const Type * override { return &modelType; }
  auto DrawUiElement() -> Error override;

  static auto wrap_SetName(lua_State *state) -> int;
  static auto wrap_GetName(lua_State *state) -> int;

  static auto wrap_SetPosition(lua_State *state) -> int;
  static auto wrap_GetPosition(lua_State *state) -> int;

  static auto wrap_SetRotation(lua_State *state) -> int;
  static auto wrap_GetRotation(lua_State *state) -> int;

  static auto wrap_SetScale(lua_State *state) -> int;
  static auto wrap_GetScale(lua_State *state) -> int;

  static auto wrap_SetUserdata(lua_State *state) -> int;
  static auto wrap_GetUserdata(lua_State *state) -> int;

  static auto LoadBinding(lua_State *state) -> int;
};

} // namespace Engine