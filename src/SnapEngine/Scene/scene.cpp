#include "scene.hpp"
#include "Modules/bindings.hpp"
#include "Modules/reflectBindings.hpp"
#include "Wrap/wrap.hpp"
#include "flecs/addons/cpp/c_types.hpp"
#include "flecs/addons/cpp/mixins/id/decl.hpp"
#include <imgui.h>
#include <vector>

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
        ImGui::Text("Component: %s", componentName);
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
  scene->world->entity(0).children(
      [&](flecs::entity entity) -> void { DrawEntityHierarchy(entity); });

  ImGui::End();

  return 0;
}

// const std::vector<luaL_Reg> SceneLib = {
//     {"drawUIElement", Scene::DrawUiElement},
// };

// extern "C" auto luaopen_Scene(lua_State *state) -> int {
//   LuaWrap::RegisterLuaType(state, Scene::GetType(), SceneLib); // NOLINT
//   return 1;
// }

const LuaWrap::LuaClass SceneLuaClass{
    .Name = "Scene",
    .Type = Scene::GetType(),
    .Methods =
        {
            {"drawUIElement", Scene::DrawUiElement},
        },
    .Children = {},
};

} // namespace Engine