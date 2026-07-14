#include "editor.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Renderer/shaderManager.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/scene.hpp"
#include "renderer.hpp"
#include <format>
#include <imgui.h>
#include <lua.h>
#include <mutex>
#include <string>
#include <string_view>

namespace Engine::Editor {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline flecs::entity SelectedEntity;

auto EntityName(const flecs::entity &entity) -> std::string_view {
  const char *entityName = entity.name();

  if (entityName == nullptr || std::string_view(entityName).empty()) {
    if (entity != flecs::ChildOf) {
      entityName = "Unnamed Component";
    } else {
      entityName = "Unnamed Entity";
    }
  }

  const auto *name = entity.try_get<DisplayName>();
  if (name != nullptr && !name->Name.empty()) {
    entityName = name->Name.c_str();
  }

  return {entityName};
}

auto DrawEntity(const flecs::entity &entity) -> bool {
  auto entityName = EntityName(entity);

  ImGui::PushID(static_cast<int>(entity.id()));

  ImGuiTreeNodeFlags nodeFlags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

  if (SelectedEntity == entity) {
    // NOLINTNEXTLINE
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  }

  bool node = ImGui::TreeNodeEx(entityName.data(), nodeFlags);
  if (ImGui::IsItemClicked()) {
    SelectedEntity = entity;
  }

  ImGui::PopID();

  return node;
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

  bool drawChildren = DrawEntity(entity);

  if (match == MatchType::This) {
    // If this entity matches, all children should be shown regardless of their names
    filter = "";
  }

  if (drawChildren) {
    ImGui::Indent();
    entity.children([&](flecs::entity child) -> void {
      DrawEntityHierarchy(child, filter);
    });
    ImGui::Unindent();
    ImGui::TreePop();
  }
}

inline auto DrawEntityEditor(flecs::entity entity, const Ref<Scene> &scene)
    -> void {
  if (!SelectedEntity.is_valid()) {
    return;
  }

  ImGui::PushID(static_cast<int>(SelectedEntity.id()));

  SelectedEntity.each([&](flecs::id identifier) -> auto {
    if (!identifier.is_entity()) {
      return;
    }

    auto componentEntity = identifier.entity();
    if (componentEntity == flecs::ChildOf || !componentEntity.is_valid()) {
      return;
    }

    auto componentName = EntityName(componentEntity);
    ImGui::TextDisabled("-%s", componentName.data());
  });

  SelectedEntity.each([&](flecs::id identifier) -> auto {
    if (!identifier.is_entity()) {
      return;
    }

    auto componentEntity = identifier.entity();

    if (componentEntity == flecs::ChildOf || !componentEntity.is_valid()) {
      return;
    }

    auto componentName = EntityName(componentEntity);

    auto iter = scene->drawFunctions.find(componentEntity.id());
    if (iter != scene->drawFunctions.end()) {
      ImGui::Separator();
      ImGui::PushID(static_cast<int>(componentEntity.id()));
      auto name = std::format("Component: {}", componentName.data());
      if (ImGui::TreeNodeEx(name.c_str(), iter->second.defaultOpen
                                              ? ImGuiTreeNodeFlags_DefaultOpen
                                              : 0)) {
        iter->second.func(SelectedEntity);
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  });

  ImGui::PopID();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto DrawSceneHierarchy(const Ref<Engine::Scene> &scene) -> Error {
  auto pickReadbackResult =
      CHECK_RES(Editor::GetEditorInstance().PopEntityPickResult());

  if (pickReadbackResult != std::nullopt) {
    auto pickResult = pickReadbackResult.value();

    scene->world.each<Geometry>(
        [&](flecs::entity entity, const Geometry &geometry) -> void {
          if (geometry.hasTlasIndex &&
              geometry.tlasIndex == pickResult.InstanceID) {
            SelectedEntity = entity;
          }
        });
  }

  constexpr float axisLength = 1.0F;
  constexpr float axisThickness = 4.0F;

  const Math::Vec3 origin{0.0F, 0.0F, 0.0F};
  auto &lineDrawer = Renderer::RendererInstance.GetLineDrawer();

  static bool DrawBoundingBox = false;
  static bool DrawBoundsRecursively = false;

  ImGui::Begin(scene->name.c_str());

  static char buf[256] = {};                    // NOLINT
  ImGui::InputText("Search", buf, sizeof(buf)); // NOLINT
  ImGui::Checkbox("Draw Bounds", &DrawBoundingBox);
  ImGui::SameLine();
  ImGui::Checkbox("Draw Bounds Recursively", &DrawBoundsRecursively);

  ImGui::Separator();
  ImGui::Text("Entity Hierarchy:");

  static float hierarchyHeightRatio = 0.5F; // NOLINT
  const float separatorHeight = 8.0F;

  auto availableHeight = ImGui::GetContentRegionAvail().y;

  if (ImGui::BeginChild(
          "Entity Hierarchy",
          ImVec2(0, availableHeight * hierarchyHeightRatio), // NOLINT
          ImGuiChildFlags_Borders)) {

    scene->world.entity(0).children([&](flecs::entity entity) -> void {
      DrawEntityHierarchy(entity, std::string_view(buf)); // NOLINT
    });
  }
  ImGui::EndChild();

  auto cursorPos = ImGui::GetCursorPos();

  ImGui::InvisibleButton(" ", ImVec2(ImGui::GetContentRegionAvail().x,
                                     ImGui::GetTextLineHeightWithSpacing()));

  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    hierarchyHeightRatio += ImGui::GetIO().MouseDelta.y / availableHeight;
    hierarchyHeightRatio =
        std::clamp(hierarchyHeightRatio, 0.1F, 0.9F); // NOLINT
  }

  ImGui::SetCursorPos(cursorPos);
  ImGui::SeparatorText("");

  if (ImGui::BeginChild("Selected Entity", ImVec2(0, 0),
                        ImGuiChildFlags_Borders)) {
    DrawEntityEditor(SelectedEntity, scene);
  }
  ImGui::EndChild();
  ImGui::End();

  return {};
}

auto Editor::PickEntity(const Camera &camera,
                        const Graphics::GraphicsContext &context,
                        Math::Vec2 mousePos) -> Error {

  auto shader =
      CHECK_RES(Renderer::RendererInstance.GetShaderManager().GetShader(
          Renderer::ShaderKey::ObjectPicker));

  auto &tlas = Renderer::RendererInstance.GetSceneTLAS();

  Graphics::BufferCreationInfo info{
      .size = sizeof(uint32_t) * 4,
      .usage =
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .stagingBuffer = true,
      .persistentMapping = true,
      .debugName = "Entity Pick Buffer",
  };

  auto pickBuffer = CHECK_RES(Graphics::Buffer::Create(context, info));

  CHECK_ERR(shader->Send(Graphics::ResourceKey{"SceneBVH"}, tlas));
  CHECK_ERR(shader->Send(Graphics::ResourceKey{"CameraData"},
                         camera.GetBuffer()->GetBuffer()));
  CHECK_ERR(shader->Send(Graphics::ResourceKey{"TraceResults"}, pickBuffer));

  CHECK_ERR(Graphics::UniformWriter::Send(
      shader, Graphics::ResourceKey{"PushConstants", "uv"}, mousePos));

  Graphics::DynamicRendering::SetShader(shader);

  CHECK_ERR(Graphics::Dispatch(context, {1, 1, 1}));

  auto readback = CHECK_RES(pickBuffer->Readback(context));

  PickEntityReadback pickReadback{
      .Buffer = pickBuffer,
      .Readback = readback,
  };

  PickedEntities.push(pickReadback);

  return {};
}

auto Editor::PopEntityPickResult() -> Result<std::optional<PickEntityResult>> {
  if (PickedEntities.empty()) {
    return std::nullopt;
  }

  auto pickReadback = PickedEntities.front();

  if (!pickReadback.Readback.isValid()) {
    PickedEntities.pop();
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(pickReadback.Readback->mutex);

  if (!pickReadback.Readback->completed) {
    return std::nullopt;
  }

  PickedEntities.pop();

  if (Error::IsError(pickReadback.Readback->error)) {
    return pickReadback.Readback->error;
  }

  auto data = pickReadback.Readback->data;

  struct PickResult {
    uint32_t instanceID;
    uint32_t primitiveID;
    uint32_t hit;
    uint32_t padding;
  };

  const auto *pickResult = // NOLINTNEXTLINE
      reinterpret_cast<const PickResult *>(data->GetData());

  if (pickResult->hit == 0) {
    return std::nullopt;
  }

  PickEntityResult pickedEntity;
  pickedEntity.PrimitiveID = pickResult->primitiveID;
  pickedEntity.InstanceID = pickResult->instanceID;

  return pickedEntity;
}

} // namespace Engine::Editor