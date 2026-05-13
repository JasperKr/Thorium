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
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/Geometry/levelOfDetail.hpp"
#include "Scene/Geometry/model.hpp"
#include "Scene/Geometry/shape.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "Scene/camera.hpp"
#include "Scene/node.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "material.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <flecs.h>
#include <imgui.h>
#include <lua.hpp>
#include <string>
#include <string_view>

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

inline auto fuzzyMatch(const std::string_view &pattern,
                       const std::string_view &str) -> bool {
  size_t index = 0;
  for (char character : str) {
    if (index < pattern.size() &&
        tolower(character) == tolower(pattern[index])) {
      ++index;
    }
  }
  return index == pattern.size();
}

enum class MatchType : uint8_t {
  This,
  Child,
  None,
};

inline auto matchesRecursive(const flecs::entity &entity,
                             const std::string_view &filter) -> MatchType {
  if (filter.empty()) {
    return MatchType::This; // If filter is empty, match all entities
  }

  if (fuzzyMatch(filter, std::string_view(entity.name()))) {
    return MatchType::This;
  }

  auto matches = MatchType::None;
  entity.children([&](flecs::entity child) -> void {
    if (matches != MatchType::None) {
      return; // If a match has already been found, skip further checks
    }

    if (matchesRecursive(child, filter) != MatchType::None) {
      matches = MatchType::Child;
    }
  });

  return matches;
}

auto DrawEntityHierarchy(const flecs::entity &entity, std::string_view filter)
    -> void {

  auto match = matchesRecursive(entity, filter);
  if (match == MatchType::None) {
    return; // Skip entities that don't match the filter
  }

  DrawEntity(entity);

  if (match == MatchType::This) {
    filter =
        ""; // If this entity matches, all children should be shown regardless of their names
  }

  entity.children([&](flecs::entity child) -> void {
    ImGui::Indent();
    DrawEntityHierarchy(child, filter);
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
  auto availableWidth = ImGui::GetContentRegionAvail().x;
  if (ImGui::BeginChild("Entity Hierarchy",
                        ImVec2(availableWidth * 0.5F, 0), // NOLINT
                        ImGuiChildFlags_Borders)) {

    static char buf[256] = {};                    // NOLINT
    ImGui::InputText("Search", buf, sizeof(buf)); // NOLINT

    scene->world.entity(0).children([&](flecs::entity entity) -> void {
      DrawEntityHierarchy(
          entity, std::string_view(buf, strnlen(buf, sizeof(buf)))); // NOLINT
    });
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

  auto albedoTexture = item.material->albedoTexture;
  if (!albedoTexture.isValid()) {
    albedoTexture = Renderer::RendererInstance.DefaultMaterial.albedoTexture;
  }

  auto metallicRoughnessTexture = item.material->metallicRoughnessTexture;
  if (!metallicRoughnessTexture.isValid()) {
    metallicRoughnessTexture =
        Renderer::RendererInstance.DefaultMaterial.metallicRoughnessTexture;
  }

  static auto albedoKey = Graphics::ResourceKey{"AlbedoTexture"};
  sendErr = shader->Send(ctx, albedoKey, albedoTexture);
  if (Error::IsError(sendErr)) {
    return sendErr;
  }
  static auto metallicRoughnessKey =
      Graphics::ResourceKey{"MetallicRoughnessTexture"};
  sendErr = shader->Send(ctx, metallicRoughnessKey, metallicRoughnessTexture);
  if (Error::IsError(sendErr)) {
    return sendErr;
  }

  static auto materialBufferIndexKey =
      Graphics::ResourceKey{"MaterialBufferIndex"};
  sendErr = Graphics::Shader::UniformWriter::Send(
      shader, ctx, materialBufferIndexKey, item.material->materialSSBOIndex);
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

  for (const auto &system : scene->preRender) {
    system.run();
  }
  scene->finalizePreRenderUploads.run();

  auto &ctx = *Graphics::GetCurrentGraphicsContext();

  const auto &shader = Graphics::DynamicRendering::GetShader();

  static auto materialBufferKey = Graphics::ResourceKey{"MaterialBuffer"};
  auto materialSendError =
      shader->Send(ctx, materialBufferKey,
                   Renderer::RendererInstance.MaterialsBuffer->GetBuffer());
  if (Error::IsError(materialSendError)) {
    return luaL_error(state, "Failed to send material buffer: %s",
                      materialSendError.message.c_str());
  }

  auto countKey = Graphics::ResourceKey{"DirectionalLightCount"};
  auto dirLightCountError = Graphics::Shader::UniformWriter::Send(
      shader, ctx, countKey,
      Renderer::RendererInstance.SceneLightBuffers.DirectionalLightCount);
  if (Error::IsError(dirLightCountError)) {
    return luaL_error(state, "Failed to send directional light count: %s",
                      dirLightCountError.message.c_str());
  }

  auto lightBufferSendError =
      Renderer::RendererInstance.BindLightBuffers(ctx, shader);
  if (Error::IsError(lightBufferSendError)) {
    return luaL_error(state, "Failed to bind light buffers: %s",
                      lightBufferSendError.message.c_str());
  }

  DrawItems.clear();
  TransparentDrawItems.clear();

  scene->world.each<Geometry>(
      [&](flecs::entity entity, const Geometry &geometry) -> void {
        entity.children([&](flecs::entity child) -> void {
          const Renderer::Material *material = nullptr;

          if (child.has<Engine::Renderer::Material>()) {
            material = child.try_get<Engine::Renderer::Material>();
          }

          if (material == nullptr) {
            material = &Renderer::RendererInstance.NoMaterial;
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

  Camera::RegisterCameraSystems(*this);

  auto &instance = Renderer::RendererInstance;
  auto &buffers = instance.SceneLightBuffers;

  preRender.emplace_back(world.system<Renderer::Material>().kind(0).each(
      [this](Renderer::Material &material) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult =
            material.Update(*Graphics::GetCurrentGraphicsContext());
      }));

  preRender.emplace_back(world.system<DirectionalLight>().kind(0).each(
      [](flecs::entity entity, const DirectionalLight &light) -> auto {
        auto error = light.Write(buffers.DirectionalLightData, entity);
      }));

  preRender.emplace_back(world.system<PointLight>().kind(0).each(
      [](flecs::entity entity, const PointLight &light) -> auto {
        auto error = light.Write(buffers.PointLightData, entity);
      }));

  preRender.emplace_back(world.system<SpotLight>().kind(0).each(
      [](flecs::entity entity, const SpotLight &light) -> auto {
        auto error = light.Write(buffers.SpotLightData, entity);
      }));

  preRender.emplace_back(world.system<RectangleLight>().kind(0).each(
      [](flecs::entity entity, const RectangleLight &light) -> auto {
        auto error = light.Write(buffers.RectangleLightData, entity);
      }));

  preRender.emplace_back(world.system<SphereLight>().kind(0).each(
      [](flecs::entity entity, const SphereLight &light) -> auto {
        auto error = light.Write(buffers.SphereLightData, entity);
      }));

  preRender.emplace_back(world.system<Camera>().kind(0).each(
      [](flecs::entity entity, const Camera &camera) -> auto {
        auto error = camera.WriteToBuffer(entity);
      }));

  finalizePreRenderUploads = world.system().kind(0).each([this]() -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    auto &ctx = *Graphics::GetCurrentGraphicsContext();

    auto error = buffers.DirectionalLightsBuffer->GetBuffer()->SetData(
        ctx, buffers.DirectionalLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.PointLightsBuffer->GetBuffer()->SetData(
        ctx, buffers.PointLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.SpotLightsBuffer->GetBuffer()->SetData(
        ctx, buffers.SpotLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.RectangleLightsBuffer->GetBuffer()->SetData(
        ctx, buffers.RectangleLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.SphereLightsBuffer->GetBuffer()->SetData(
        ctx, buffers.SphereLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }
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
            {"newModel", LuaModel::Create},
            {"newShape", LuaShape::Create},
            {"newLOD", LuaLevelOfDetail::Create},
            {"newGeometry", LuaGeometry::Create},
            {"newDirectionalLight", LuaDirectionalLight::Create},
            {"newPointLight", LuaPointLight::Create},
            {"newSpotLight", LuaSpotLight::Create},
            {"newRectangleLight", LuaRectangleLight::Create},
            {"newSphereLight", LuaSphereLight::Create},
            {"newCamera", LuaCamera::Create},
        },
    .Children = {},
};

} // namespace Engine