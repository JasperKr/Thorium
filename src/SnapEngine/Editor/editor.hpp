#pragma once

#include "Graphics/buffer.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/camera.hpp"
#include "Scene/scene.hpp"
#include <flecs.h>
#include <optional>
#include <queue>
namespace Engine::Editor {

enum class MatchType : uint8_t {
  This,
  Child,
  None,
};

auto DrawEntity(const flecs::entity &entity) -> bool;
auto DrawEntityHierarchy(const flecs::entity &entity, std::string_view filter)
    -> void;
auto DrawSceneHierarchy(const Ref<Engine::Scene> &scene) -> Error;
auto EntityName(const flecs::entity &entity) -> std::string_view;

struct PickEntityReadback {
  Ref<Graphics::Buffer> Buffer;
  Ref<Graphics::BufferReadback> Readback;
};

struct PickEntityResult {
  uint32_t PrimitiveID{};
  uint32_t InstanceID{};
};

struct Editor {
  std::queue<PickEntityReadback> PickedEntities;

  // Mouse position is expected to be within the range [0, 1]
  auto PickEntity(const Camera &camera,
                  const Graphics::GraphicsContext &context, Math::Vec2 mousePos)
      -> Error;

  auto PopEntityPickResult() -> Result<std::optional<PickEntityResult>>;

  static auto GetEditorInstance() -> Editor & {
    static Editor instance;
    return instance;
  }
};

} // namespace Engine::Editor