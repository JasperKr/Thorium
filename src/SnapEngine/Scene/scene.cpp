#include "scene.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/reflectBindings.hpp"
#include "Scene/geometry.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Scene/model.hpp"
#include "Scene/shape.hpp"
#include "Wrap/wrap.hpp"
#include "flecs/addons/cpp/c_types.hpp"
#include "flecs/addons/cpp/mixins/id/decl.hpp"
#include <imgui.h>
#include <lauxlib.h>
#include <string>

namespace Engine {

auto Scene::LoadBinding(lua_State *state) -> int {
  Bindings::LuaBoundStruct<Scene> bindings("Scene");
  bindings.RegisterMember<&Scene::name>("Name");
  bindings.DocumentCustomMethod(Bindings::MethodInfo{
      .name = "DrawUiElement",
      .description = "Draws a UI element for this scene."});
  bindings.Register(state);

  return 0;
}

auto DrawEntityHierarchy(const flecs::entity &entity) -> void {
  const char *entityName = entity.name();

  if (strcmp(entityName, "flecs") == 0) {
    return; // Skip internal flecs root entity
  }

  if (entityName == nullptr || std::string_view(entityName).empty()) {
    entityName = "Unnamed Entity";
  }

  ImGui::Text("%s", entityName);

  entity.each([&](flecs::id identifier) -> auto {
    if (identifier.is_entity()) {
      auto componentEntity = identifier.entity();

      if (componentEntity != flecs::ChildOf) {
        const char *componentName = componentEntity.name();
        if (componentName == nullptr ||
            std::string_view(componentName).empty()) {
          componentName = "Unnamed Component";
        }
        ImGui::TextDisabled(" - %s", componentName);
      }
    }
  });

  entity.children([&](flecs::entity child) -> void {
    ImGui::Indent();
    DrawEntityHierarchy(child);
    ImGui::Unindent();
  });
}

auto Scene::DrawUiElement(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  // Draw an imgui hierarchy of the scene's entities and components
  // This is just a placeholder for now

  ImGui::Begin(scene->name.c_str());
  scene->world->entity().children(
      [&](flecs::entity entity) -> void { DrawEntityHierarchy(entity); });

  ImGui::End();

  return 0;
}

auto Scene::DrawModels(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  auto *ctx = Graphics::GetCurrentGraphicsContext();
  Error drawResult = Error::Success();

  scene->world->each<Geometry>(
      [&](flecs::entity entity, const Geometry &geometry) -> void {
        auto result = Graphics::Draw(*ctx, *geometry.mesh);
        if (Error::IsError(result)) {
          drawResult = result;
        }
      });

  if (Error::IsError(drawResult)) {
    return luaL_error(state, "%s", drawResult.ToString().c_str());
  }

  return 0;
}

const LuaWrap::LuaClass SceneLuaClass{
    .Name = "Scene",
    .Type = Scene::GetType(),
    .Methods =
        {
            {"drawUIElement", Scene::DrawUiElement},
            {"drawModels", Scene::DrawModels},
            {"createModel", LuaModel::Create},
            {"createShape", LuaShape::Create},
            {"createLOD", LuaLevelOfDetail::Create},
            {"createGeometry", LuaGeometry::Create},
        },
    .Children = {},
};

} // namespace Engine