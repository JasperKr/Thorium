#include "geometry.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <imgui.h>

namespace Engine {

auto Geometry::DrawGUI() const -> void {
  if (mesh.isValid()) {
    ImGui::Text("Mesh: %p", mesh.get());
  } else {
    ImGui::Text("Mesh: None");
  }
}

auto LuaGeometry::GetMesh(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Expected a Geometry object");
  }

  auto geometry = entity->get<Geometry>();

  ::LuaWrap::PushObject(state, Graphics::Mesh::GetType(), geometry.mesh.get());
  return 1;
}

auto LuaGeometry::Create(lua_State *state) -> int {
  auto *scene = ::LuaWrap::ObjectFromLua<Scene>(state, 1);
  const char *name = luaL_checkstring(state, 2);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a World object");
  }

  auto *mesh = ::LuaWrap::ObjectFromLua<Graphics::Mesh>(state, 3);
  if (mesh == nullptr) {
    return luaL_error(state, "Expected a Mesh object");
  }

  auto meshRef = Ref<Graphics::Mesh>(mesh);

  auto entity = scene->world.entity(name);
  entity.set<Geometry>(Geometry{meshRef});
  entity.add<Transform>();

  auto luaGeometry = LuaGeometry::FromEntity(entity);
  ::LuaWrap::PushObject(state, LuaGeometry::GetType(), luaGeometry.get());

  return 1;
}

const ::LuaWrap::LuaClass GeometryClass = {
    .Name = "Geometry",
    .Type = LuaGeometry::GetType(),
    .Methods =
        {
            {"getMesh", LuaGeometry::GetMesh},
            {"create", LuaGeometry::Create},
        },
};

} // namespace Engine