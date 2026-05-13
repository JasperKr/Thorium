#pragma once

#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"
namespace Engine {

struct Camera {
  void SetVerticalFOV(Math::Scalar fov) {
    VerticalFOV = fov;
    dirty = true;
  }

  void SetAspectRatio(Math::Scalar aspectRatio) {
    AspectRatio = aspectRatio;
    dirty = true;
  }

  void SetNearPlane(Math::Scalar nearPlane) {
    NearPlane = nearPlane;
    dirty = true;
  }

  void SetFarPlane(Math::Scalar farPlane) {
    FarPlane = farPlane;
    dirty = true;
  }

  [[nodiscard]] auto GetAspectRatio() const -> Math::Scalar {
    return AspectRatio;
  }

  [[nodiscard]] auto GetVerticalFOV() const -> Math::Scalar {
    return VerticalFOV;
  }

  [[nodiscard]] auto GetNearPlane() const -> Math::Scalar { return NearPlane; }

  [[nodiscard]] auto GetFarPlane() const -> Math::Scalar { return FarPlane; }

  static void RegisterCameraSystems(Scene &scene);

  static auto Create(const Graphics::GraphicsContext &context,
                     Math::Scalar verticalFOV, Math::Ivec2 Dimensions,
                     Math::Scalar near, Math::Scalar far) -> Result<Camera>;

  auto WriteToBuffer(flecs::entity entity) const -> Error;

private:
  Math::Scalar VerticalFOV{};
  Math::Ivec2 Dimensions;
  Math::Scalar AspectRatio{};
  Math::Scalar NearPlane{};
  Math::Scalar FarPlane{};

  Ref<Graphics::StructuredBuffer> CameraBuffer;

  bool dirty = true;
};

extern const Graphics::BufferFormat CameraBufferFormat;

static const Type CameraType = Type("Camera");

struct LuaCamera : Object {
  explicit LuaCamera(flecs::entity entity) : entity(entity) {}

  static auto Create(lua_State *state) -> int;

  flecs::entity entity;

  static auto GetType() -> const Type * { return &CameraType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override {
    return LuaCamera::GetType();
  }

  static auto GetName(lua_State *state) -> int;
  static auto SetName(lua_State *state) -> int;

  static auto GetVerticalFOV(lua_State *state) -> int;
  static auto SetVerticalFOV(lua_State *state) -> int;

  static auto GetAspectRatio(lua_State *state) -> int;
  static auto SetAspectRatio(lua_State *state) -> int;

  static auto GetNearPlane(lua_State *state) -> int;
  static auto SetNearPlane(lua_State *state) -> int;

  static auto GetFarPlane(lua_State *state) -> int;
  static auto SetFarPlane(lua_State *state) -> int;
};

auto GetLuaCameraClass() -> LuaWrap::LuaClass;

} // namespace Engine