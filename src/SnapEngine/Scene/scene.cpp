#include "scene.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/reflectBindings.hpp"
#include "Scene/boundingBox.hpp"
#include "Scene/geometry.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Scene/model.hpp"
#include "Scene/node.hpp"
#include "Scene/shape.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "flecs/addons/cpp/c_types.hpp"
#include "flecs/addons/cpp/mixins/id/decl.hpp"
#include "material.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <imgui.h>
#include <lauxlib.h>
#include <lua.hpp>
#include <string>

namespace Engine {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline flecs::entity SelectedEntity;

auto Scene::LoadBinding(lua_State *state) -> int {
  Bindings::LuaBoundStruct<Scene> bindings("Scene");
  bindings.RegisterMember<&Scene::name>("Name");
  bindings.DocumentCustomMethod(Bindings::MethodInfo{
      .name = "DrawUiElement",
      .description = "Draws a UI element for this scene."});
  bindings.Register(state);

  return 0;
}

auto DrawEntity(const flecs::entity &entity) -> void {
  const char *entityName = entity.name();

  if (strcmp(entityName, "flecs") == 0) {
    return; // Skip internal flecs root entity
  }

  if (entityName == nullptr || std::string_view(entityName).empty()) {
    if (entity != flecs::ChildOf) {
      entityName = "Unnamed Component";
    } else {
      entityName = "Unnamed Entity";
    }
  }

  ImGui::Text("%s", entityName);
  if (ImGui::IsItemClicked()) {
    SelectedEntity = entity;
  }

  entity.each([&](flecs::id identifier) -> auto {
    if (identifier.is_entity()) {
      auto componentEntity = identifier.entity();

      if (componentEntity != flecs::ChildOf && componentEntity.is_valid()) {
        const char *componentName = componentEntity.name();
        if (componentName == nullptr ||
            std::string_view(componentName).empty()) {
          componentName = "Unnamed Component";
        }
        ImGui::TextDisabled(" - %s", componentName);

        if (ImGui::IsItemClicked()) {
          SelectedEntity = componentEntity;
        }
      }
    }
  });
}

auto DrawEntityHierarchy(const flecs::entity &entity) -> void {
  DrawEntity(entity);

  entity.children([&](flecs::entity child) -> void {
    ImGui::Indent();
    DrawEntityHierarchy(child);
    ImGui::Unindent();
  });
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Scene::DrawUiElement(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  // Draw an imgui hierarchy of the scene's entities and components
  // This is just a placeholder for now

  ImGui::Begin(scene->name.c_str());
  if (ImGui::BeginChild("Entity Hierarchy", ImVec2(300, 0), // NOLINT
                        ImGuiChildFlags_Borders)) {
    scene->world.entity(0).children(
        [&](flecs::entity entity) -> void { DrawEntityHierarchy(entity); });
  }
  ImGui::EndChild();
  ImGui::SameLine();

  if (ImGui::BeginChild("Selected Entity", ImVec2(0, 0),
                        ImGuiChildFlags_Borders)) {
    if (SelectedEntity.is_valid()) {
      ImGui::Separator();

      DrawEntity(SelectedEntity);

      SelectedEntity.each([&](flecs::id identifier) -> auto {
        if (identifier.is_entity()) {
          auto componentEntity = identifier.entity();

          if (componentEntity != flecs::ChildOf && componentEntity.is_valid()) {
            const char *componentName = componentEntity.name();
            if (componentName == nullptr ||
                std::string_view(componentName).empty()) {
              componentName = "Unnamed Component";
            }
          }
        }
      });

      if (SelectedEntity.has<Transform>() &&
          SelectedEntity.get_ref<Transform>().get() != nullptr) {
        auto transform = SelectedEntity.get_ref<Transform>();
        transform->DrawGUI();
      }

      if (SelectedEntity.has<Geometry>() &&
          SelectedEntity.get_ref<Geometry>().get() != nullptr) {
        auto geometry = SelectedEntity.get_ref<Geometry>();
        geometry->DrawGUI();
      }

      if (SelectedEntity.has<LevelOfDetail>() &&
          SelectedEntity.get_ref<LevelOfDetail>().get() != nullptr) {
        auto lod = SelectedEntity.get_ref<LevelOfDetail>();
        lod->DrawGUI();
      }

      // if (SelectedEntity.has<Model>() &&
      //     SelectedEntity.get_ref<Model>().get() != nullptr) {
      //   auto model = SelectedEntity.get_ref<Model>();
      //   model->DrawGUI();
      // }

      if (SelectedEntity.has<Userdata>() &&
          SelectedEntity.get_ref<Userdata>().get() != nullptr) {
        auto userdata = SelectedEntity.get_ref<Userdata>();
        userdata->DrawGUI(state);
      }

      if (SelectedEntity.has<BoundingBox>() &&
          SelectedEntity.get_ref<BoundingBox>().get() != nullptr) {
        auto boundingBox = SelectedEntity.get_ref<BoundingBox>();
        boundingBox->DrawGUI();
      }

      if (SelectedEntity.has<LocalBounds>() &&
          SelectedEntity.get_ref<LocalBounds>().get() != nullptr) {
        auto localBounds = SelectedEntity.get_ref<LocalBounds>();
        localBounds->DrawGUI();
      }

      if (SelectedEntity.has<WorldBounds>() &&
          SelectedEntity.get_ref<WorldBounds>().get() != nullptr) {
        auto worldBounds = SelectedEntity.get_ref<WorldBounds>();
        worldBounds->DrawGUI();
      }

      if (SelectedEntity.has<Renderer::Material>() &&
          SelectedEntity.get_ref<Renderer::Material>().get() != nullptr) {
        auto material = SelectedEntity.get_ref<Renderer::Material>();
        material->DrawGUI();
      }
    }
  }
  ImGui::EndChild();
  ImGui::End();

  return 0;
}

struct DrawItem {
  flecs::entity geom_entity;
  Geometry geometry;
  const Renderer::Material *material{};

  uint64_t primaryKey;
  uint64_t secondaryKey;
  uint64_t tertiaryKey;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<DrawItem> DrawItems;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<DrawItem> TransparentDrawItems;

inline auto CompareDrawItems(const DrawItem &first, const DrawItem &second)
    -> bool {
  if (first.primaryKey != second.primaryKey) {
    return first.primaryKey < second.primaryKey;
  }
  if (first.secondaryKey != second.secondaryKey) {
    return first.secondaryKey < second.secondaryKey;
  }
  return first.tertiaryKey < second.tertiaryKey;
}

inline auto RenderDrawItem(const DrawItem &item,
                           const Ref<Graphics::Shader::ShaderModule> &shader,
                           Graphics::GraphicsContext &ctx) -> Error {
  const auto &worldMatrix = item.geom_entity.get<Transform>().GetWorldMatrix();
  const auto &normalMatrix = Math::Matrix3x3(worldMatrix).InverseTranspose();

  static auto modelMatrixKey = Graphics::ResourceKey{"ModelMatrix"};
  static auto normalMatrixKey = Graphics::ResourceKey{"NormalMatrix"};

  auto sendErr = Graphics::Shader::UniformWriter::Send(
      shader, ctx, modelMatrixKey, worldMatrix);

  if (Error::IsError(sendErr)) {
    return sendErr;
  }

  sendErr = Graphics::Shader::UniformWriter::Send(shader, ctx, normalMatrixKey,
                                                  normalMatrix);

  if (Error::IsError(sendErr)) {
    return sendErr;
  }

  if (!item.material->albedoTexture.isValid()) {
    return Error::Create("Invalid material texture");
  }

  static auto materialKey = Graphics::ResourceKey{"MainTexture"};
  sendErr = shader->Send(ctx, materialKey, item.material->albedoTexture);
  if (Error::IsError(sendErr)) {
    return sendErr;
  }

  if (item.geometry.mesh.get() == nullptr) {
    return Error::Create("Invalid geometry mesh");
  }

  auto result = Graphics::Draw(ctx, *item.geometry.mesh);
  if (Error::IsError(result)) {
    return result;
  }

  return {};
}

auto Scene::DrawModels(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  auto &ctx = *Graphics::GetCurrentGraphicsContext();

  const auto &shader = Graphics::DynamicRendering::GetShader();
  DrawItems.clear();
  TransparentDrawItems.clear();

  scene->world.each<Geometry>(
      [&](flecs::entity entity, const Geometry &geometry) -> void {
        entity.children([&](flecs::entity child) -> void {
          const Renderer::Material *material = nullptr;

          if (child.has<Engine::Renderer::Material>()) {
            material = child.try_get<Engine::Renderer::Material>();
          }

          if (material == nullptr || !material->albedoTexture.isValid()) {
            material = &Renderer::RendererInstance.DefaultMaterial;
          }

          if (material->alphaMode != Renderer::AlphaMode::Blend) {
            DrawItems.emplace_back(
                DrawItem{.geom_entity = entity,
                         .geometry = geometry,
                         .material = material,
                         .primaryKey = material->GetMainSortKey(),
                         .secondaryKey = geometry.mesh->GetHash(),
                         .tertiaryKey = material->GetSecondarySortKey()});
          } else {
            TransparentDrawItems.emplace_back(
                DrawItem{.geom_entity = entity,
                         .geometry = geometry,
                         .material = material,
                         .primaryKey = material->GetMainSortKey(),
                         .secondaryKey = geometry.mesh->GetHash(),
                         .tertiaryKey = material->GetSecondarySortKey()});
          }
        });
      });

  std::ranges::sort(DrawItems, CompareDrawItems);
  std::ranges::sort(TransparentDrawItems, CompareDrawItems);

  for (const auto &item : DrawItems) {
    auto err = RenderDrawItem(item, shader, ctx);
    if (Error::IsError(err)) {
      return luaL_error(state, "Failed to render draw item: %s",
                        err.message.c_str());
    }
  }

  for (const auto &item : TransparentDrawItems) {
    auto err = RenderDrawItem(item, shader, ctx);
    if (Error::IsError(err)) {
      return luaL_error(state, "Failed to render transparent draw item: %s",
                        err.message.c_str());
    }
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
      world.system<Engine::Transform, Engine::Transform *>()
          .term_at(1)
          .parent()
          .cascade()
          .each([](Transform &transform, Transform *parentTransform) -> auto {
            transform.UpdateLocalMatrix();
            transform.UpdateWorldMatrix(parentTransform);
          });

  auto preBoundsCascadeSystem = world.system<Engine::WorldBounds>().each(
      [](Engine::WorldBounds &bbox) -> auto { bbox.Bounds.Reset(); });

  preBoundsCascadeSystem.depends_on(transformSystem);

  auto boundingBoxSystem =
      world
          .system<Engine::Transform, Engine::LocalBounds, Engine::WorldBounds>()
          .each([](flecs::entity entity, Engine::Transform &transform,
                   Engine::LocalBounds &bbox,
                   Engine::WorldBounds &wbbox) -> auto {
            wbbox.Bounds.Construct(transform, bbox.Bounds);
          });
  boundingBoxSystem.depends_on(preBoundsCascadeSystem);

  auto postBoundingboxSystem =
      world.system<Engine::WorldBounds, Engine::WorldBounds *>()
          .term_at(1)
          .parent()
          .cascade()
          .each([](Engine::WorldBounds &bbox,
                   Engine::WorldBounds *parentBbox) -> auto {
            if (parentBbox != nullptr) {
              parentBbox->Bounds.UnionInPlace(bbox.Bounds);
            }
          });

  postBoundingboxSystem.depends_on(boundingBoxSystem);

  auto preDrawSystem = world.system<Renderer::Material>().each(
      [this](Renderer::Material &material) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult =
            material.Update(*Graphics::GetCurrentGraphicsContext());
      });
}

auto Scene::Update(double deltaTime) const -> Error {
  world.progress(static_cast<float>(deltaTime));

  return lastUpdateResult;
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