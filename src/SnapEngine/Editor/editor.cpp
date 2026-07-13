#include "editor.hpp"
#include "Modules/console.hpp"
#include "Scene/scene.hpp"
#include "renderer.hpp"
#include <format>
#include <imgui.h>
#include <lua.h>

namespace Engine::Editor {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline flecs::entity SelectedEntity;

auto DrawEntity(const flecs::entity &entity) -> void {
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

  ImGui::PushID(static_cast<int>(entity.id()));

  ImGuiTreeNodeFlags nodeFlags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

  if (SelectedEntity == entity) {
    // NOLINTNEXTLINE
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  }

  bool node = ImGui::TreeNodeEx(entityName, nodeFlags);
  if (ImGui::IsItemClicked()) {
    SelectedEntity = entity;
  }

  if (node) {
    ImGui::Indent();
    entity.each([&](flecs::id identifier) -> auto {
      if (identifier.is_entity()) {
        auto componentEntity = identifier.entity();

        if (componentEntity != flecs::ChildOf && componentEntity.is_valid()) {
          const char *componentName = componentEntity.name();
          if (componentName == nullptr ||
              std::string_view(componentName).empty()) {
            componentName = "Unnamed Component";
          }
          ImGui::TextDisabled("-%s", componentName);
        }
      }
    });

    ImGui::Unindent();
    ImGui::TreePop();
  }

  ImGui::PopID();
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
auto DrawSceneHierarchy(const Engine::Scene &scene) -> Error {
  constexpr float axisLength = 1.0F;
  constexpr float axisThickness = 4.0F;

  const Math::Vec3 origin{0.0F, 0.0F, 0.0F};
  auto &lineDrawer = Renderer::RendererInstance.GetLineDrawer();

  // lineDrawer.OverlayLine(origin, Math::Vec3{axisLength, 0.0F, 0.0F},
  //                        Math::Vec4{1.0F, 0.0F, 0.0F, 1.0F}, axisThickness);
  // lineDrawer.OverlayLine(origin, Math::Vec3{0.0F, axisLength, 0.0F},
  //                        Math::Vec4{0.0F, 1.0F, 0.0F, 1.0F}, axisThickness);
  // lineDrawer.OverlayLine(origin, Math::Vec3{0.0F, 0.0F, axisLength},
  //                        Math::Vec4{0.0F, 0.0F, 1.0F, 1.0F}, axisThickness);

  static bool DrawBoundingBox = false;
  static bool DrawBoundsRecursively = false;

  ImGui::Begin(scene.name.c_str());

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

    scene.world.entity(0).children([&](flecs::entity entity) -> void {
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
    if (SelectedEntity.is_valid()) {
      ImGui::PushID(static_cast<int>(SelectedEntity.id()));

      SelectedEntity.each([&](flecs::id identifier) -> auto {
        if (identifier.is_entity()) {
          auto componentEntity = identifier.entity();

          if (componentEntity != flecs::ChildOf && componentEntity.is_valid()) {
            const char *componentName = componentEntity.name();
            if (componentName == nullptr ||
                std::string_view(componentName).empty()) {
              componentName = "Unnamed Component";
            }

            auto iter = scene.drawFunctions.find(componentEntity.id());
            if (iter != scene.drawFunctions.end()) {
              ImGui::Separator();
              ImGui::PushID(static_cast<int>(componentEntity.id()));
              auto name = std::format("Component: {}", componentName);
              if (ImGui::TreeNodeEx(name.c_str(),
                                    iter->second.defaultOpen
                                        ? ImGuiTreeNodeFlags_DefaultOpen
                                        : 0)) {
                iter->second.func(SelectedEntity);
                ImGui::TreePop();
              }
              ImGui::PopID();
            }
          }
        }
      });

      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  ImGui::End();

  return {};
}
} // namespace Engine::Editor