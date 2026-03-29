#pragma once
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "flecs.h"
#include <lua.hpp>
#include <string>
#include <utility>

namespace Engine {

static const Type SceneType = Type("Scene");

struct Scene : Object {
  flecs::world *world{};
  std::string name;

  explicit Scene(std::string name)
      : world(new flecs::world()), name(std::move(name)) {}

  static auto GetType() -> const Type * { return &SceneType; }
  auto GetInstanceType() const -> const Type * override { return &SceneType; }

  static auto LoadBinding(lua_State *state) -> int;
  static auto DrawUiElement(lua_State *state) -> int;
  static auto DrawModels(lua_State *state) -> int;
};

extern const LuaWrap::LuaClass SceneLuaClass;

} // namespace Engine