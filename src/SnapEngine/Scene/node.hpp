#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "drawable.hpp"
#include "lua.hpp"
#include "sceneObject.hpp"
#include "selectable.hpp"
#include "transform.hpp"
#include <string>
#include <vector>

namespace Engine {

const Type nodeType = Type("Node");

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

} // namespace Engine