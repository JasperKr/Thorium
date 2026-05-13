#include "camera.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "Scene/cameraMatrices.hpp"
#include "Scene/displayName.hpp"
#include "Scene/frustum.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include <flecs.h>
#include <lauxlib.h>
#include <span>

namespace Engine {

const auto CameraBufferFormat = Graphics::BufferFormat({
    {"ViewMatrix", "floatmat4"},
    {"InverseViewMatrix", "floatmat4"},
    {"ProjectionMatrix", "floatmat4"},
    {"InverseProjectionMatrix", "floatmat4"},
    {"ViewProjectionMatrix", "floatmat4"},
    {"InverseViewProjectionMatrix", "floatmat4"},
    {"RotationProjectionMatrix", "floatmat4"},
    {"InverseRotationProjectionMatrix", "floatmat4"},
    {"Position", "floatvec3"},
    {"Near", "float"},
    {"Far", "float"},
    {"NearMulFar", "float"},
    {"FarMinusNear", "float"},
    {"HistoryInvalidated", "uint32"},
    {"Jitter", "floatvec2"},
    {"ProjectionType", "uint32"},
    {"ShadowCascadeCount", "uint32"},
});

struct CameraBufferStruct {
  Math::Matrix4x4 ViewMatrix;
  Math::Matrix4x4 InverseViewMatrix;
  Math::Matrix4x4 ProjectionMatrix;
  Math::Matrix4x4 InverseProjectionMatrix;
  Math::Matrix4x4 ViewProjectionMatrix;
  Math::Matrix4x4 InverseViewProjectionMatrix;
  Math::Matrix4x4 RotationProjectionMatrix;
  Math::Matrix4x4 InverseRotationProjectionMatrix;
  Math::Vec3 Position{};
  Math::Scalar Near{};
  Math::Scalar Far{};
  Math::Scalar NearMulFar{};
  Math::Scalar FarMinusNear{};
  uint32_t HistoryInvalidated{};
  Math::Vec2 Jitter;
  uint32_t ProjectionType{};
  uint32_t ShadowCascadeCount{};
};

void Camera::RegisterCameraSystems(Scene &scene) {
  auto cameraProjection =
      scene.world.system<Camera, CameraMatrices>()
          .kind(flecs::PostUpdate)
          .each([](flecs::entity entity, Camera &camera,
                   CameraMatrices &matrices) -> void {
            if (camera.dirty) {
              matrices.ProjectionMatrix = Math::Matrix4x4::Perspective(
                  camera.VerticalFOVRad, camera.AspectRatio, camera.NearPlane,
                  camera.FarPlane);
              matrices.InverseProjectionMatrix =
                  matrices.ProjectionMatrix.Inverse();

              camera.dirty = false;
            }
          });

  auto cameraTransform =
      scene.world.system<CameraMatrices, Transform>()
          .kind(flecs::PostUpdate)
          .term_at(1)
          .detect_changes()
          .each([](flecs::entity entity, CameraMatrices &matrices,
                   Transform &transform) -> void {
            const auto &worldMatrix = transform.GetWorldMatrix();

            matrices.ViewMatrix = worldMatrix.Inverse();
            matrices.InverseViewMatrix = worldMatrix;
            matrices.ViewProjectionMatrix =
                matrices.ProjectionMatrix * matrices.ViewMatrix;
            matrices.InverseViewProjectionMatrix =
                matrices.ViewProjectionMatrix.Inverse();
            matrices.RotationProjectionMatrix =
                matrices.ProjectionMatrix * worldMatrix.AsMatrix3x3();
            matrices.InverseRotationProjectionMatrix =
                matrices.RotationProjectionMatrix.Inverse();
          });

  auto cameraFrustum =
      scene.world.system<CameraMatrices, Frustum>()
          .kind(flecs::PostUpdate)
          .each([](flecs::entity entity, CameraMatrices &matrices,
                   Frustum &frustum) -> void {
            frustum =
                Frustum::FromMatrices(matrices.ViewProjectionMatrix,
                                      matrices.InverseViewProjectionMatrix);
          });

  cameraFrustum.depends_on(cameraTransform);
  cameraTransform.depends_on(cameraProjection);
}

auto Camera::Create(const Graphics::GraphicsContext &context,
                    Math::Scalar verticalFOVDeg, Math::Ivec2 Dimensions,
                    Math::Scalar near, Math::Scalar far) -> Result<Camera> {
  Camera camera;
  camera.verticalFOVDeg = verticalFOVDeg;
  camera.VerticalFOVRad = Math::DegToRad(verticalFOVDeg);
  camera.Dimensions = Dimensions;
  camera.AspectRatio = static_cast<Math::Scalar>(Dimensions.x) /
                       static_cast<Math::Scalar>(Dimensions.y);
  camera.NearPlane = near;
  camera.FarPlane = far;

  Graphics::StructuredBufferCreationInfo info{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags = static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) |
                    static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
      .debugName = "Camera Buffer",
  };

  auto bufferResult =
      Graphics::StructuredBuffer::Create(context, CameraBufferFormat, 2, info);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error().AsUnexpected();
  }

  camera.CameraBuffer = bufferResult.value();

  return camera;
}

// scene:newCamera(name, verticalFOV, width, height, near, far)
auto LuaCamera::Create(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid Scene object");
  }

  auto context = *Graphics::GetCurrentGraphicsContext();

  const auto *name = luaL_optstring(state, 2, "Camera");
  const auto fov = luaL_optscalar(state, 3, 60.0);
  const auto width = luaL_optint(state, 4, Window::GetWidth(context.sdlWindow));
  const auto height =
      luaL_optint(state, 5, Window::GetHeight(context.sdlWindow));
  const auto near = luaL_optscalar(state, 6, 0.1F);
  const auto far = luaL_optscalar(state, 7, 1000.0F);

  auto cameraResult = Camera::Create(context, fov, {width, height}, near, far);

  if (Error::IsError(cameraResult)) {
    return luaL_error(state, "Failed to create Camera: %s",
                      cameraResult.error().message.c_str());
  }

  auto entity = scene->world.entity();
  entity.set<Camera>(cameraResult.value());
  entity.add<CameraMatrices>();
  entity.add<Frustum>();
  entity.add<Transform>();
  entity.set<DisplayName>({luaL_optstring(state, 2, "Camera")});

  auto camera = Ref<LuaCamera>::Make(entity);

  LuaWrap::PushObject(state, LuaCamera::GetType(), camera.get());
  return 1;
}

auto LuaCamera::GetName(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  const auto *displayName = obj->entity.try_get<DisplayName>();
  if (displayName == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushstring(state, displayName->Name.c_str());
  return 1;
}

auto LuaCamera::SetName(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  auto *displayName = obj->entity.try_get_mut<DisplayName>();
  if (displayName == nullptr) {
    obj->entity.set<DisplayName>({luaL_checkstring(state, 2)});
  } else {
    displayName->Name = luaL_checkstring(state, 2);
  }

  return 0;
}

auto LuaCamera::SetAspectRatio(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  auto *camera = obj->entity.try_get_mut<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  camera->SetAspectRatio(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::SetVerticalFOV(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  auto *camera = obj->entity.try_get_mut<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  camera->SetVerticalFOV(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::SetNearPlane(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  auto *camera = obj->entity.try_get_mut<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  camera->SetNearPlane(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::SetFarPlane(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  auto *camera = obj->entity.try_get_mut<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  camera->SetFarPlane(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::GetAspectRatio(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  const auto *camera = obj->entity.try_get<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  lua_pushnumber(state, camera->GetAspectRatio());
  return 1;
}

auto LuaCamera::GetVerticalFOV(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  const auto *camera = obj->entity.try_get<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  lua_pushnumber(state, camera->GetVerticalFOV());
  return 1;
}

auto LuaCamera::GetNearPlane(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  const auto *camera = obj->entity.try_get<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  lua_pushnumber(state, camera->GetNearPlane());
  return 1;
}

auto LuaCamera::GetFarPlane(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  const auto *camera = obj->entity.try_get<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  lua_pushnumber(state, camera->GetFarPlane());
  return 1;
}

auto Camera::WriteToBuffer(flecs::entity entity) const -> Error {
  auto context = *Graphics::GetCurrentGraphicsContext();

  assert(sizeof(CameraBufferStruct) == CameraBuffer->GetStride());

  auto result = CameraBuffer->GetBuffer()->CopyTo(
      context, *CameraBuffer->GetBuffer(), 0, sizeof(CameraBufferStruct),
      sizeof(CameraBufferStruct));
  if (Error::IsError(result)) {
    return result;
  }

  const auto *cameraMatrices = entity.try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return Error::Create("CameraMatrices component not found on entity");
  }

  const auto *transform = entity.try_get<Transform>();
  if (transform == nullptr) {
    return Error::Create("Transform component not found on entity");
  }

  auto data = CameraBufferStruct{
      .ViewMatrix = cameraMatrices->ViewMatrix,
      .InverseViewMatrix = cameraMatrices->InverseViewMatrix,
      .ProjectionMatrix = cameraMatrices->ProjectionMatrix,
      .InverseProjectionMatrix = cameraMatrices->InverseProjectionMatrix,
      .ViewProjectionMatrix = cameraMatrices->ViewProjectionMatrix,
      .InverseViewProjectionMatrix =
          cameraMatrices->InverseViewProjectionMatrix,
      .RotationProjectionMatrix = cameraMatrices->RotationProjectionMatrix,
      .InverseRotationProjectionMatrix =
          cameraMatrices->InverseRotationProjectionMatrix,
      .Position = transform->GetPosition(),
      .Near = NearPlane,
      .Far = FarPlane,
      .NearMulFar = NearPlane * FarPlane,
      .FarMinusNear = FarPlane - NearPlane,
      .HistoryInvalidated = 0,
      .Jitter = {0.0F, 0.0F},
      .ProjectionType = 0,
      .ShadowCascadeCount = 0,
  };

  auto span = std::span<CameraBufferStruct>(&data, 1);

  return CameraBuffer->GetBuffer()->SetData(context, span);
}

auto LuaCamera::GetBuffer(lua_State *state) -> int {
  auto *obj = LuaWrap::ObjectFromLua<LuaCamera>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Camera object");
  }

  const auto *camera = obj->entity.try_get<Camera>();
  if (camera == nullptr) {
    return luaL_error(state, "Camera component not found on entity");
  }

  LuaWrap::PushObject(state, Graphics::StructuredBuffer::GetType(),
                      camera->GetBuffer().get());
  return 1;
}

auto GetLuaCameraClass() -> LuaWrap::LuaClass {
  const LuaWrap::LuaClass LuaCameraClass = {
      .Name = "Camera",
      .Type = LuaCamera::GetType(),
      .Methods =
          {
              {"getName", LuaCamera::GetName},
              {"setName", LuaCamera::SetName},
              {"getAspectRatio", LuaCamera::GetAspectRatio},
              {"setAspectRatio", LuaCamera::SetAspectRatio},
              {"getVerticalFOV", LuaCamera::GetVerticalFOV},
              {"setVerticalFOV", LuaCamera::SetVerticalFOV},
              {"getNearPlane", LuaCamera::GetNearPlane},
              {"setNearPlane", LuaCamera::SetNearPlane},
              {"getFarPlane", LuaCamera::GetFarPlane},
              {"setFarPlane", LuaCamera::SetFarPlane},
              {"getBuffer", LuaCamera::GetBuffer},
          },
      .Components = {
          TransformComponent,
          CameraMatricesComponent,
          FrustumComponent,
          DisplayNameComponent,
      }};

  return LuaCameraClass;
}

} // namespace Engine