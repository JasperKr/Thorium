#pragma once

#include "Modules/Engine/drawable.hpp"
#include "Modules/Engine/identifier.hpp"
#include "Modules/Engine/scene.hpp"
#include "Modules/object.hpp"
#include <string>
#include <vector>

#include "lua.hpp"

namespace Engine {

const Type projectType = Type("Project");

struct Project : Object, UiElement {
  std::vector<Ref<Scene>> scenes;
  std::string name;
  Identifier id = GenerateIdentifier();

  Project() = delete;
  ~Project() override = default;
  Project(const Project &) = delete;
  Project(Project &&) = delete;
  auto operator=(const Project &) -> Project & = delete;
  auto operator=(Project &&) -> Project & = delete;
  explicit Project(std::string name) : name(std::move(name)) {}

  static auto GetType() -> Type const * { return &projectType; }
  auto GetInstanceType() const -> const Type * override { return &projectType; }

  static auto GetScene(lua_State *state) -> int;
  static auto AddScene(lua_State *state) -> int;
  static auto RemoveScene(lua_State *state) -> int;
  static auto LoadBinding(lua_State *state) -> int;
  auto DrawUiElement() -> Error override;
};

} // namespace Engine