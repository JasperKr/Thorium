#include "scene.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/reflectBindings.hpp"
#include "Renderer/lightProbe.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/Geometry/levelOfDetail.hpp"
#include "Scene/Geometry/model.hpp"
#include "Scene/Geometry/shape.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/light.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "Scene/camera.hpp"
#include "Scene/environment.hpp"
#include "Scene/frustum.hpp"
#include "Scene/node.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include "material.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <flecs.h>
#include <imgui.h>
#include <lua.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "../Snap/Modules/Peripherals/keyboard.hpp"

namespace Engine {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline flecs::entity SelectedEntity;

auto LuaScene::LoadBinding(lua_State *state) -> int {
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

  const char *entityName = entity.name();

  if (strcmp(entityName, "flecs") == 0 || strcmp(entityName, "Engine") == 0) {
    return; // Skip internal flecs root entity
  }

  // Skip systems
  if (entity.has(flecs::System)) {
    return;
  }

  DrawEntity(entity);

  if (match == MatchType::This) {
    // If this entity matches, all children should be shown regardless of their names
    filter = "";
  }

  entity.children([&](flecs::entity child) -> void {
    ImGui::Indent();
    DrawEntityHierarchy(child, filter);
    ImGui::Unindent();
  });
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Scene::DrawUiElement() const -> Error {
  constexpr float axisLength = 1.0F;
  constexpr float axisThickness = 4.0F;

  const Math::Vec3 origin{0.0F, 0.0F, 0.0F};
  auto &lineDrawer = Renderer::RendererInstance.GetLineDrawer();

  lineDrawer.OverlayLine(origin, Math::Vec3{axisLength, 0.0F, 0.0F},
                         Math::Vec4{1.0F, 0.0F, 0.0F, 1.0F}, axisThickness);
  lineDrawer.OverlayLine(origin, Math::Vec3{0.0F, axisLength, 0.0F},
                         Math::Vec4{0.0F, 1.0F, 0.0F, 1.0F}, axisThickness);
  lineDrawer.OverlayLine(origin, Math::Vec3{0.0F, 0.0F, axisLength},
                         Math::Vec4{0.0F, 0.0F, 1.0F, 1.0F}, axisThickness);

  ImGui::Begin(name.c_str());
  auto availableHeight = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginChild("Entity Hierarchy",
                        ImVec2(0, availableHeight * 0.5F), // NOLINT
                        ImGuiChildFlags_Borders)) {

    static char buf[256] = {};                    // NOLINT
    ImGui::InputText("Search", buf, sizeof(buf)); // NOLINT

    world.entity(0).children([&](flecs::entity entity) -> void {
      DrawEntityHierarchy(entity, std::string_view(buf)); // NOLINT
    });
  }
  ImGui::EndChild();

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

      if (SelectedEntity.has<Userdata>() &&
          SelectedEntity.get_ref<Userdata>().get() != nullptr) {
        auto userdata = SelectedEntity.get_ref<Userdata>();
        userdata->DrawGUI(nullptr);
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

      if (SelectedEntity.has<Renderer::LightProbe>() &&
          SelectedEntity.get_ref<Renderer::LightProbe>().get() != nullptr) {
        auto lightProbe = SelectedEntity.get_ref<Renderer::LightProbe>();
        CHECK_ERR(lightProbe->DrawGui(SelectedEntity));
      }

      if (SelectedEntity.has<Engine::Light>() &&
          SelectedEntity.get_ref<Engine::Light>().get() != nullptr) {
        auto light = SelectedEntity.get_ref<Engine::Light>();
        light->DrawGUI();
      }
    }
  }
  ImGui::EndChild();
  ImGui::End();

  return {};
}

struct DrawItem {
  flecs::entity geom_entity;
  Geometry geometry;
  const Renderer::Material *material{};
  uint32_t transformIndex{};

  uint64_t primaryKey;
  uint64_t secondaryKey;
  uint64_t tertiaryKey;
};

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

inline auto BindMaterial(const Ref<Graphics::Shader> &shader,
                         Graphics::GraphicsContext &ctx,
                         const Renderer::Material *material, uint8_t flags)
    -> Error {
  const auto &defaultMaterial = Renderer::RendererInstance.GetDefaultMaterial();

  auto albedoTexture = material->albedoTexture;
  if (!albedoTexture.isValid()) {
    albedoTexture = defaultMaterial.albedoTexture;
  }

  auto metallicRoughnessTexture = material->metallicRoughnessTexture;
  if (!metallicRoughnessTexture.isValid()) {
    metallicRoughnessTexture = defaultMaterial.metallicRoughnessTexture;
  }

  auto ambientOcclusionTexture = material->ambientOcclusionTexture;
  if (!ambientOcclusionTexture.isValid()) {
    ambientOcclusionTexture = defaultMaterial.ambientOcclusionTexture;
  }

  auto normalTexture = material->normalTexture;
  if (!normalTexture.isValid()) {
    normalTexture = defaultMaterial.normalTexture;
  }

  auto emissiveTexture = material->emissiveTexture;
  if (!emissiveTexture.isValid()) {
    emissiveTexture = defaultMaterial.emissiveTexture;
  }
  auto reflectanceTexture = material->reflectanceTexture;
  if (!reflectanceTexture.isValid()) {
    reflectanceTexture = defaultMaterial.reflectanceTexture;
  }

  // NOLINTBEGIN
  if ((flags & 1U) != 0U) {
    static auto albedoKey = Graphics::ResourceKey{"AlbedoTexture"};
    CHECK_ERR(shader->Send(albedoKey, albedoTexture));
  }

  if ((flags & 2U) != 0U) {
    static auto metallicRoughnessKey =
        Graphics::ResourceKey{"MetallicRoughnessTexture"};
    CHECK_ERR(shader->Send(metallicRoughnessKey, metallicRoughnessTexture));
  }

  // if ((flags & 4U) != 0U) {
  //   static auto ambientOcclusionTextureKey =
  //       Graphics::ResourceKey{"AmbientOcclusionTexture"};

  //   CHECK_ERR(
  //       shader->Send(ambientOcclusionTextureKey, ambientOcclusionTexture));
  // }

  if ((flags & 8U) != 0U) {
    static auto normalTextureKey = Graphics::ResourceKey{"NormalTexture"};
    CHECK_ERR(shader->Send(normalTextureKey, normalTexture));
  }

  if ((flags & 16U) != 0U) {
    static auto emissiveTextureKey = Graphics::ResourceKey{"EmissiveTexture"};
    CHECK_ERR(shader->Send(emissiveTextureKey, emissiveTexture));
  }

  if ((flags & 32U) != 0U) {
    static auto reflectanceTextureKey =
        Graphics::ResourceKey{"ReflectanceTexture"};
    CHECK_ERR(shader->Send(reflectanceTextureKey, reflectanceTexture));
  }
  // NOLINTEND

  return {};
}

struct DrawConfig {
  /*
    albedoTexture
    metallicRoughnessTexture
    ambientOcclusionTexture
    normalTexture
    emissiveTexture
    reflectanceTexture
  */
  uint8_t bindMaterialTextures{};
  bool bindMaterialBuffer = false;
};

inline auto RenderDrawItem(const DrawItem &item,
                           const Ref<Graphics::Shader> &shader,
                           Graphics::GraphicsContext &ctx,
                           const DrawConfig &config) -> Error {
  if (config.bindMaterialTextures != 0) {
    CHECK_ERR(
        BindMaterial(shader, ctx, item.material, config.bindMaterialTextures));
  }

  if (config.bindMaterialBuffer) {
    static auto materialBufferIndexKey =
        Graphics::ResourceKey{"PushConstants", "MaterialBufferIndex"};
    CHECK_ERR(Graphics::UniformWriter::Send(shader, materialBufferIndexKey,
                                            item.material->materialSSBOIndex));
  }

  static auto modelTransformIndexKey =
      Graphics::ResourceKey{"PushConstants", "ModelTransformIndex"};
  CHECK_ERR(Graphics::UniformWriter::Send(shader, modelTransformIndexKey,
                                          item.transformIndex));

  if (item.geometry.mesh.get() == nullptr) {
    return Error::Create("Invalid geometry mesh");
  }

  Graphics::DynamicRendering::SetCullMode(item.material->cullMode);

  CHECK_ERR(Graphics::Draw(ctx, *item.geometry.mesh));

  return {};
}

inline auto AddDrawItem(std::vector<DrawItem> &OpaqueDrawItems,
                        std::vector<DrawItem> &MaskedDrawItems,
                        std::vector<DrawItem> &TransparentDrawItems,
                        flecs::entity entity, const Geometry &geometry,
                        const Frustum &frustum) -> void {
  const Renderer::Material *material = nullptr;
  material = entity.try_get<Renderer::Material>();

  if (material == nullptr) {
    material = &Renderer::RendererInstance.GetNoMaterial();
  }

  const auto &worldBounds = entity.get<WorldBounds>();
  const auto &boundingBox = worldBounds.Bounds;

  if (!frustum.IntersectsAABB(boundingBox)) {
    return; // Skip entities that are outside the camera frustum
  }

  auto drawItem = DrawItem{.geom_entity = entity,
                           .geometry = geometry,
                           .material = material,
                           .primaryKey = material->GetMainSortKey(),
                           .secondaryKey = geometry.mesh->GetHash(),
                           .tertiaryKey = material->GetSecondarySortKey()};

  switch (material->alphaMode) {
  case Renderer::AlphaMode::Opaque:
    OpaqueDrawItems.emplace_back(drawItem);
    break;
  case Renderer::AlphaMode::Mask:
    MaskedDrawItems.emplace_back(drawItem);
    break;
  case Renderer::AlphaMode::Blend:
    TransparentDrawItems.emplace_back(drawItem);
    break;
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Scene::DrawModels(Camera &camera, Frustum &frustum,
                       const Graphics::GraphicsContext &context) -> Error {
  // Pre-frame setup

  Renderer::RendererInstance.NewFrame();

  for (const auto &system : preRender) {
    system.run();
  }
  finalizePreRenderUploads.run();

  // Shader configuration

  auto &ctx = *Graphics::GetCurrentGraphicsContext();
  auto &textures = camera.GetOwnedTextures();

  auto depthOpaque = CHECK_RES(
      Renderer::RendererInstance.GetShader(Renderer::ShaderKey::DepthPrepass));
  auto depthMasked = CHECK_RES(
      Renderer::RendererInstance.GetShader(Renderer::ShaderKey::DepthMaskPass));
  auto deferred = CHECK_RES(
      Renderer::RendererInstance.GetShader(Renderer::ShaderKey::Deferred));
  auto forward = CHECK_RES(Renderer::RendererInstance.GetShader(
      Renderer::ShaderKey::TransparencyForward));

  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};
  auto cameraBuffer = camera.GetBuffer()->GetBuffer();

  CHECK_ERR(Graphics::UniformWriter::Send(depthOpaque, cameraBufferKey,
                                          cameraBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(depthMasked, cameraBufferKey,
                                          cameraBuffer));
  CHECK_ERR(
      Graphics::UniformWriter::Send(deferred, cameraBufferKey, cameraBuffer));
  CHECK_ERR(
      Graphics::UniformWriter::Send(forward, cameraBufferKey, cameraBuffer));

  static auto modelTransformsBufferKey =
      Graphics::ResourceKey{"ModelTransforms"};

  auto modelTransformsBuffer =
      Renderer::RendererInstance.GetModelTransformsBuffer()->GetBuffer();
  CHECK_ERR(Graphics::UniformWriter::Send(depthOpaque, modelTransformsBufferKey,
                                          modelTransformsBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(depthMasked, modelTransformsBufferKey,
                                          modelTransformsBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(deferred, modelTransformsBufferKey,
                                          modelTransformsBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(forward, modelTransformsBufferKey,
                                          modelTransformsBuffer));

  static auto materialBufferKey = Graphics::ResourceKey{"MaterialBuffer"};
  auto materialBuffer =
      Renderer::RendererInstance.GetMaterialsBuffer()->GetBuffer();
  CHECK_ERR(depthMasked->Send(materialBufferKey, materialBuffer));
  CHECK_ERR(deferred->Send(materialBufferKey, materialBuffer));
  CHECK_ERR(forward->Send(materialBufferKey, materialBuffer));

  // Draw sorting

  static std::vector<DrawItem> OpaqueDrawItems;
  static std::vector<DrawItem> MaskedDrawItems;
  static std::vector<DrawItem> TransparentDrawItems;

  snap_defer(OpaqueDrawItems.clear());
  snap_defer(MaskedDrawItems.clear());
  snap_defer(TransparentDrawItems.clear());

  world.each<Geometry>(
      [&](flecs::entity entity, const Geometry &geometry) -> void {
        bool hasChildren = false;
        entity.children([&](flecs::entity child) -> void {
          hasChildren = true;

          AddDrawItem(OpaqueDrawItems, MaskedDrawItems, TransparentDrawItems,
                      child, geometry, frustum);
        });

        if (!hasChildren) {
          AddDrawItem(OpaqueDrawItems, MaskedDrawItems, TransparentDrawItems,
                      entity, geometry, frustum);
        }
      });

  std::ranges::sort(OpaqueDrawItems, CompareDrawItems);
  std::ranges::sort(MaskedDrawItems, CompareDrawItems);
  std::ranges::sort(TransparentDrawItems, CompareDrawItems);

  // Prepare model transform buffer

  struct ModelTransformData {
    std::array<float, 16> modelMatrix{};  // NOLINT
    std::array<float, 16> normalMatrix{}; // NOLINT

    constexpr explicit ModelTransformData(Transform transform) {
      std::memcpy(modelMatrix.data(), // NOLINT
                  transform.GetWorldMatrix().floatData(),
                  sizeof(float) * Math::Matrix4x4::Size);

      std::memcpy(normalMatrix.data(), // NOLINT
                  Math::Matrix4x4(transform.GetNormalMatrix()).floatData(),
                  sizeof(float) * Math::Matrix4x4::Size);
    }
  };

  std::vector<ModelTransformData> modelTransforms;
  size_t transformCount = 0UL;
  modelTransforms.reserve(OpaqueDrawItems.size() + MaskedDrawItems.size() +
                          TransparentDrawItems.size());

  for (auto &item : OpaqueDrawItems) {
    item.transformIndex = transformCount++;
    modelTransforms.emplace_back(item.geom_entity.get<Transform>());
  }

  for (auto &item : MaskedDrawItems) {
    item.transformIndex = transformCount++;
    modelTransforms.emplace_back(item.geom_entity.get<Transform>());
  }

  for (auto &item : TransparentDrawItems) {
    item.transformIndex = transformCount++;
    modelTransforms.emplace_back(item.geom_entity.get<Transform>());
  }

  CHECK_ERR(Renderer::RendererInstance.AssureModelTransformBufferSize(
      transformCount));

  auto span = std::span<const ModelTransformData>(modelTransforms.data(),
                                                  modelTransforms.size());

  CHECK_ERR(Renderer::RendererInstance.GetModelTransformsBuffer()
                ->GetBuffer()
                ->SetData(ctx, span));

  // Depth Opaque prepass

  const auto &rendertargets = camera.GetRendertargets();

  textures.Depth =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Depth));

  Graphics::DynamicRendering::SetDepthMode(true, true, VK_COMPARE_OP_GREATER);
  Graphics::DynamicRendering::SetShader(depthOpaque);

  CHECK_ERR(Graphics::DynamicRendering::SetRenderTargets(
      ctx, {{
               .clearValue = VkClearValue{0.0F, 0},
               .texture = textures.Depth,
               .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
           }}));

  static auto opaqueConfig = DrawConfig{
      .bindMaterialTextures = 0,
      .bindMaterialBuffer = false,
  };

  Graphics::PushDebugMarker("Depth Opaque Prepass");

  for (const auto &item : OpaqueDrawItems) {
    CHECK_ERR(RenderDrawItem(item, depthOpaque, ctx, opaqueConfig));
  }

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Depth Masked Prepass");

  static auto maskedConfig = DrawConfig{
      .bindMaterialTextures = 1,
      .bindMaterialBuffer = true,
  };
  Graphics::DynamicRendering::SetShader(depthMasked);
  for (const auto &item : MaskedDrawItems) {
    CHECK_ERR(RenderDrawItem(item, depthMasked, ctx, maskedConfig));
  }

  Graphics::PopDebugMarker();

  textures.Albedo =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Albedo));
  textures.Normal =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Normal));
  textures.Material =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Material));
  textures.Emissive =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Emissive));
  textures.Motion =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Motion));

  Graphics::DynamicRendering::SetDepthMode(true, false, VK_COMPARE_OP_EQUAL);
  Graphics::DynamicRendering::SetShader(deferred);

  CHECK_ERR(Graphics::DynamicRendering::SetRenderTargets(
      ctx, {{
                .texture = textures.Depth,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Albedo,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Normal,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Material,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Emissive,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Motion,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            }}));

  static auto deferredConfig = DrawConfig{
      .bindMaterialTextures = UINT8_MAX,
      .bindMaterialBuffer = true,
  };

  Graphics::PushDebugMarker("Opaque Materials");

  for (const auto &item : OpaqueDrawItems) {
    CHECK_ERR(RenderDrawItem(item, deferred, ctx, deferredConfig));
  }

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Masked Materials");

  for (const auto &item : MaskedDrawItems) {
    CHECK_ERR(RenderDrawItem(item, deferred, ctx, deferredConfig));
  }

  Graphics::PopDebugMarker();

  if (OpaqueDrawItems.empty() && MaskedDrawItems.empty()) {
    CHECK_ERR(Graphics::DynamicRendering::Clear(
        ctx, {
                 .colors =
                     {
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Albedo
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Normal
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Material
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Emissive
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Motion
                     },
                 .depthClearValue = 0.0F,
                 .clearDepth = true,
             }));
  }

  auto shader = CHECK_RES(Renderer::RendererInstance.GetShader(
      Renderer::ShaderKey::SimpleLighting));
  Graphics::DynamicRendering::SetShader(shader);
  CHECK_ERR(shader->Send(cameraBufferKey, cameraBuffer));
  static auto albedoTextureKey = Graphics::ResourceKey{"AlbedoTexture"};
  static auto normalTextureKey = Graphics::ResourceKey{"NormalTexture"};
  static auto materialTextureKey = Graphics::ResourceKey{"MaterialTexture"};
  static auto emissiveTextureKey = Graphics::ResourceKey{"EmissiveTexture"};
  static auto depthBufferKey = Graphics::ResourceKey{"DepthTexture"};

  CHECK_ERR(shader->Send(albedoTextureKey, textures.Albedo));
  CHECK_ERR(shader->Send(normalTextureKey, textures.Normal));
  CHECK_ERR(shader->Send(materialTextureKey, textures.Material));
  CHECK_ERR(shader->Send(emissiveTextureKey, textures.Emissive));
  CHECK_ERR(shader->Send(depthBufferKey, textures.Depth));

  auto countKey = Graphics::ResourceKey{"DirectionalLightCount"};
  CHECK_ERR(Graphics::UniformWriter::Send(
      shader, countKey,
      Renderer::RendererInstance.GetSceneLightBuffers().DirectionalLightCount));
  CHECK_ERR(Graphics::UniformWriter::Send(
      forward, countKey,
      Renderer::RendererInstance.GetSceneLightBuffers().DirectionalLightCount));

  CHECK_ERR(Renderer::RendererInstance.BindLightBuffers(ctx, shader));
  CHECK_ERR(Renderer::RendererInstance.BindLightBuffers(ctx, forward));

  textures.DirectLighting =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.DirectLighting));

  CHECK_ERR(Graphics::DynamicRendering::SetRenderTargets(
      context, {{
                   .texture = textures.DirectLighting,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               }}));

  // TODO: Remove Normals, Motion and Material here once we need them for post-processing effects
  CHECK_ERR(Renderer::GlobalRenderTargetManager.ReleaseRendertargets({
      textures.Albedo,
      textures.Normal,
      textures.Material,
      textures.Emissive,
      textures.Motion,
  }));

  // Now left: IncomingLight and Depth

  CHECK_ERR(Renderer::DrawFullScreen(context));

  CHECK_ERR(Graphics::DynamicRendering::SetRenderTargets(
      ctx, {{
                .texture = textures.Depth,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            },
            {
                .blendMode = Graphics::DefaultBlendMode,
                .texture = textures.DirectLighting,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            }}));

  static auto forwardConfig = DrawConfig{
      .bindMaterialTextures = UINT8_MAX,
      .bindMaterialBuffer = true,
  };

  Graphics::PushDebugMarker("Transparent Materials");

  // Skybox should be drawn before transparent objects, this is currently wrong.
  Graphics::DynamicRendering::SetDepthMode(true, false, VK_COMPARE_OP_GREATER);
  Graphics::DynamicRendering::SetShader(forward);
  Graphics::DynamicRendering::SetCullMode(VK_CULL_MODE_NONE);
  for (const auto &item : TransparentDrawItems) {
    CHECK_ERR(RenderDrawItem(item, forward, ctx, forwardConfig));
  }

  Graphics::PopDebugMarker();

  // Otherwise meshes won't be destroyed due to living in these vectors
  OpaqueDrawItems.clear();
  MaskedDrawItems.clear();
  TransparentDrawItems.clear();

  return {};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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
          .up()      // Start at parents
          .cascade() // Breadth-first
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
          .up()      // Start at parents
          .cascade() // Breadth-first
          .desc()    // Reverse order, so children are processed before parents
          .each([](Engine::WorldBounds &bbox,
                   Engine::WorldBounds *parentBbox) -> auto {
            if (parentBbox != nullptr) {
              parentBbox->Bounds.UnionInPlace(bbox.Bounds);
            }
          });

  postBoundingboxSystem.depends_on(boundingBoxSystem);

  Camera::RegisterCameraSystems(*this);

  auto &instance = Renderer::RendererInstance;
  auto &buffers = instance.GetSceneLightBuffers();

  preRender.emplace_back(world.system<Renderer::Material>().kind(0).each(
      [this](Renderer::Material &material) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult =
            material.Update(*Graphics::GetCurrentGraphicsContext());
      }));

  preRender.emplace_back(world.system<DirectionalLight>().kind(0).each(
      [this, &buffers](flecs::entity entity,
                       const DirectionalLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.DirectionalLightData, entity);
      }));

  preRender.emplace_back(world.system<PointLight>().kind(0).each(
      [this, &buffers](flecs::entity entity, const PointLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.PointLightData, entity);
      }));

  preRender.emplace_back(world.system<SpotLight>().kind(0).each(
      [this, &buffers](flecs::entity entity, const SpotLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.SpotLightData, entity);
      }));

  preRender.emplace_back(world.system<RectangleLight>().kind(0).each(
      [this, &buffers](flecs::entity entity,
                       const RectangleLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.RectangleLightData, entity);
      }));

  preRender.emplace_back(world.system<SphereLight>().kind(0).each(
      [this, &buffers](flecs::entity entity, const SphereLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.SphereLightData, entity);
      }));

  preRender.emplace_back(world.system<Camera>().kind(0).each(
      [this](flecs::entity entity, const Camera &camera) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        auto &transform = entity.get_mut<Transform>();
        auto &cameraMatrices = entity.get_mut<CameraMatrices>();

        lastUpdateResult = camera.WriteToBuffer(cameraMatrices, transform);
      }));

  finalizePreRenderUploads =
      world.system().kind(0).each([this, &buffers]() -> auto {
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
  Renderer::GlobalRenderTargetManager.Update();

  return lastUpdateResult;
}

auto Scene::SetEnvironment(flecs::entity environment) -> void {
  currentEnvironment = environment;
}

auto Scene::GetEnvironment() const -> flecs::entity {
  return currentEnvironment;
}

auto LuaScene::Update(lua_State *state) -> int {
  auto *scene = ::LuaWrap::ObjectFromLua<Scene>(state, 1);

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

auto LuaScene::DrawUiElement(lua_State *state) -> int {
  auto *scene = ::LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  auto drawResult = scene->DrawUiElement();
  if (Error::IsError(drawResult)) {
    return luaL_error(state, "%s", drawResult.ToString().c_str());
  }

  return 0;
}

auto LuaScene::SetEnvironment(lua_State *state) -> int {
  auto *scene = ::LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  auto *environment = ::LuaWrap::EntityFromLua(state, 2);
  if (environment == nullptr) {
    return luaL_error(state, "Expected an Environment object");
  }

  scene->SetEnvironment(*environment);

  return 0;
}

const ::LuaWrap::LuaClass SceneLuaClass{
    .Name = "Scene",
    .Type = Scene::GetType(),
    .Methods =
        {
            {"drawUIElement", LuaScene::DrawUiElement},
            {"update", LuaScene::Update},
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
            {"setEnvironment", LuaScene::SetEnvironment},
            {"newEnvironment", LuaEnvironment::Create},
            {"newLightProbe", Renderer::LuaLightProbe::Create},
        },
    .Children = {},
};

} // namespace Engine