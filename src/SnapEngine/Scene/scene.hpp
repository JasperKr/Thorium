#pragma once
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include <flecs.h>
#include <lua.hpp>
#include <string>
#include <vector>

namespace Engine {

static const Type SceneType = Type("Scene");

struct Scene : Object {
  flecs::world world;
  std::vector<flecs::system> preRender;
  flecs::system finalizePreRenderUploads;
  std::string name;
  Error lastUpdateResult = Error::Success();

  explicit Scene();
  explicit Scene(std::string name);

  static auto GetType() -> const Type * { return &SceneType; }
  auto GetInstanceType() const -> const Type * override { return &SceneType; }

  static auto LoadBinding(lua_State *state) -> int;
  static auto DrawUiElement(lua_State *state) -> int;
  static auto DrawModels(lua_State *state) -> int;
  static auto Update(lua_State *state) -> int;

  auto Update(double deltaTime) const -> Error;
  auto UpdateTransforms() const -> void;
  auto UpdateBoundingBoxes() const -> void;
};

extern const LuaWrap::LuaClass SceneLuaClass;

} // namespace Engine