#pragma once

#include "Graphics/buffer.hpp"
#include "Modules/Math/ray.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/camera.hpp"
#include "Scene/scene.hpp"
#include <flecs.h>
#include <optional>
#include <queue>
#include <vector>
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

enum class TransformAxis : uint8_t {
  None,
  X,
  Y,
  Z,
  XY,
  XZ,
  YZ,
  XYZ,
};

enum class TransformMode : uint8_t {
  None,
  Translate,
  Rotate,
  Scale,
};

struct MoveData {
  std::vector<flecs::entity> Entities;
  TransformAxis Axis = TransformAxis::None;
  TransformMode Mode = TransformMode::None;
  Math::Ray OriginalRay;
  float OriginalDistance = 0.0F;
  Math::Vec3 Origin{};
  float Distance{};
  Math::Vec2 StartMousePosition;
  Math::Vec2 CurrentMousePosition;
};

void GizmoTranslation(const MoveData &moveData, const Math::Ray &currentRay,
                      Transform &transform);

void GizmoRotation(const MoveData &moveData, const Math::Ray &currentRay,
                   Transform &transform);

void GizmoScale(const MoveData &moveData, Transform &transform);

void TransformGizmo(const MoveData &moveData, const Math::Ray &currentRay,
                    Transform &transform);

struct Editor {
  std::queue<PickEntityReadback> PickedEntities;
  MoveData CurrentMoveData;

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