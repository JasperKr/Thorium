#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "drawable.hpp"
#include "lua.hpp"
#include "material.hpp"
#include "selectable.hpp"
#include "transform.hpp"

namespace Engine {

const Type shapeType = Type("Shape");

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
} // namespace Engine