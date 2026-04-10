#include "scene.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/bindings.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/reflectBindings.hpp"
#include "Scene/boundingBox.hpp"
#include "Scene/geometry.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Scene/model.hpp"
#include "Scene/shape.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "flecs/addons/cpp/c_types.hpp"
#include "flecs/addons/cpp/mixins/id/decl.hpp"
#include <imgui.h>
#include <lauxlib.h>
#include <lua.hpp>
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
  scene->world.entity(0).children(
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

  auto shader = Graphics::DynamicRendering::GetShader();

  scene->world.each<Geometry>([&](flecs::entity entity,
                                  const Geometry &geometry) -> void {
    auto worldMatrix = entity.get<Transform>().GetWorldMatrix();
    auto normalMatrix = Math::Matrix3x3(worldMatrix).InverseTranspose();

    auto sendErr = shader->Send(*ctx, {"ModelMatrix"}, worldMatrix.byteSpan());

    if (Error::IsError(sendErr)) {
      drawResult = sendErr;
      return;
    }

    // TODO: We do not automatically account for internal matrix padding, so the shader expects
    // float3x3 but we need to send a float4x3 since std140 layout rules require each row to be aligned to a vec4.
    sendErr = shader->Send(*ctx, {"NormalMatrix"}, normalMatrix.byteSpan());

    if (Error::IsError(sendErr)) {
      drawResult = sendErr;
      return;
    }

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

Scene::Scene(std::string name) : name(std::move(name)) {
  world.component<Geometry>();
  world.component<LocalBounds>();
  world.component<WorldBounds>();
  world.component<Transform>();
  world.component<LevelOfDetail>();
  world.component<Model>();
  world.component<Node>();
  world.component<Shape>();
  world.component<Userdata>();

  auto transformSystem =
      world.system<Engine::Transform>()
          .cascade(flecs::ChildOf) // ensures parent first
          .each([](flecs::entity entity, Transform &transform) -> auto {
            transform.UpdateLocalMatrix();
            if (auto parent = entity.parent()) {
              const auto &parentTransform = parent.get<Transform>();
              transform.UpdateWorldMatrix(&parentTransform);
            } else {
              transform.UpdateWorldMatrix(nullptr);
            }
          });

  auto boundingBoxSystem =
      world
          .system<Engine::Transform, Engine::LocalBounds, Engine::WorldBounds>()
          .each([](flecs::entity entity, Engine::Transform &transform,
                   Engine::LocalBounds &bbox,
                   Engine::WorldBounds &wbbox) -> auto {
            wbbox.Bounds.Construct(transform, bbox.Bounds);
          });
  boundingBoxSystem.depends_on(transformSystem);
}

auto Scene::Update(double deltaTime) const -> Error {
  world.progress(static_cast<float>(deltaTime));

  return Error::Success();
}

auto Scene::Update(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  auto deltaTime = luaL_checknumber(state, 2);
  auto updateResult = scene->Update(deltaTime);
  if (Error::IsError(updateResult)) {
    return luaL_error(state, "%s", updateResult.ToString().c_str());
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
            {"update", Scene::Update},
            {"createModel", LuaModel::Create},
            {"createShape", LuaShape::Create},
            {"createLOD", LuaLevelOfDetail::Create},
            {"createGeometry", LuaGeometry::Create},
        },
    .Children = {},
};

} // namespace Engine