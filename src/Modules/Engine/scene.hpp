#pragma once

#include "Modules/Engine/drawable.hpp"
#include "Modules/Engine/identifier.hpp"
#include "Modules/Engine/model.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <string>
#include <vector>

#include "lua.hpp"

namespace Engine {

const Type sceneType = Type("Scene");

struct Scene : Object, UiElement {
  Scene() = delete;
  ~Scene() override = default;
  Scene(const Scene &) = delete;
  Scene(Scene &&) = delete;
  auto operator=(const Scene &) -> Scene & = delete;
  auto operator=(Scene &&) -> Scene & = delete;
  explicit Scene(std::string name) : name(std::move(name)) {}

  std::vector<SceneObject> hierarchy;
  std::string name;
  Identifier id = GenerateIdentifier();

  auto GetSceneObject(lua_State *state) -> int;

  static auto LoadBinding(lua_State *state) -> int;
  static auto GetHierarchyObjects(lua_State *state) -> int;
  static auto AddHierarchyObject(lua_State *state) -> int;
  static auto GetHierarchyObject(lua_State *state) -> int;
  static auto RemoveHierarchyObject(lua_State *state) -> int;

  static auto GetType() -> Type const * { return &sceneType; }
  auto GetInstanceType() const -> const Type * override { return &sceneType; }

  auto DrawUiElement() -> Error override;
};

} // namespace Engine