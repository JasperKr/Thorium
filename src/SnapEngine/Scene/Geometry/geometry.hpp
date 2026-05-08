#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include "flecs.h"
#include <lua.hpp>
#include <utility>
namespace Engine {

static const Type geometryType = Type("Geometry");

struct Geometry {
  Ref<Graphics::Mesh> mesh;

  auto DrawGUI() const -> void;
};

struct LuaGeometry : Object {
  flecs::entity entity;

  explicit LuaGeometry(const flecs::entity &entity) : entity(entity) {}

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaGeometry> {
    return Ref<LuaGeometry>::Make(entity);
  }

  static auto GetType() -> const Type * { return &geometryType; }
  auto GetInstanceType() const -> const Type * override {
    return &geometryType;
  }

  static auto GetMesh(lua_State *state) -> int;
  static auto Create(lua_State *state) -> int;
};

extern const LuaWrap::LuaClass GeometryClass;

} // namespace Engine