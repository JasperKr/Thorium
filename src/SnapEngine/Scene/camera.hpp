#pragma once

#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
namespace Engine {

struct Camera {
  friend struct LuaCamera;

  void SetVerticalFOV(Math::Scalar fovDeg) {
    verticalFOVDeg = fovDeg;
    VerticalFOVRad = Math::DegToRad(fovDeg);
    projectionDirty = true;
  }

  void SetAspectRatio(Math::Scalar aspectRatio) {
    AspectRatio = aspectRatio;
    projectionDirty = true;
  }

  void SetNearPlane(Math::Scalar nearPlane) {
    NearPlane = nearPlane;
    projectionDirty = true;
  }

  void SetFarPlane(Math::Scalar farPlane) {
    FarPlane = farPlane;
    projectionDirty = true;
  }

  [[nodiscard]] auto GetAspectRatio() const -> Math::Scalar {
    return AspectRatio;
  }

  [[nodiscard]] auto GetVerticalFOV() const -> Math::Scalar {
    return verticalFOVDeg;
  }

  [[nodiscard]] auto GetNearPlane() const -> Math::Scalar { return NearPlane; }

  [[nodiscard]] auto GetFarPlane() const -> Math::Scalar { return FarPlane; }

  [[nodiscard]] auto GetBuffer() const -> Ref<Graphics::StructuredBuffer> {
    return CameraBuffer;
  }

  static void RegisterCameraSystems(Scene &scene);

  static auto Create(const Graphics::GraphicsContext &context,
                     Math::Scalar verticalFOVDeg, Math::Uvec2 Dimensions,
                     Math::Scalar near, Math::Scalar far) -> Result<Camera>;

  auto WriteToBuffer(flecs::entity entity) const -> Error;

  auto Resize(Math::Uvec2 newDimensions) -> void {
    Dimensions = newDimensions;
    projectionDirty = true;
  }

  auto Render(const Graphics::GraphicsContext &context,
              flecs::entity thisEntity, Scene *scene) -> Error;

  // Descriptors for the textures we'd like to own.
  struct CameraRendertargets {
    Renderer::RendertargetDescriptor Depth;
    Renderer::RendertargetDescriptor IncomingLight;
    Renderer::RendertargetDescriptor PostProcessed;
  };

  // References to the textures we currently own. Dynamic
  struct AllocatedTextures {
    Ref<Graphics::Texture> Depth;
    Ref<Graphics::Texture> IncomingLight;
    Ref<Graphics::Texture> PostProcessed;
  };

  struct PostProcessingConfig {
    float Temperature = 0.0F;
    float Tint = 0.0F;
    bool ApplyAGX = true;
    float Contrast = 1.0F;
    float Saturation = 1.0F;
    float Vignette = 0.0F;
    float Exposure = 0.2F; // NOLINT
  };

  auto SetPostProcessingConfig(const PostProcessingConfig &config) -> void {
    postProcessingConfig = config;
  }

  [[nodiscard]] auto GetPostProcessingConfig() const -> PostProcessingConfig {
    return postProcessingConfig;
  }

private:
  Math::Scalar verticalFOVDeg{};
  Math::Scalar VerticalFOVRad{};
  Math::Uvec2 Dimensions;
  Math::Scalar AspectRatio{};
  Math::Scalar NearPlane{};
  Math::Scalar FarPlane{};

  Ref<struct Graphics::StructuredBuffer> CameraBuffer;

  CameraRendertargets Rendertargets;
  AllocatedTextures OwnedTextures;
  PostProcessingConfig postProcessingConfig;

  bool projectionDirty = true;

  auto ConfigureRendertargets() -> void;

  auto ApplyPostProcessing(const Graphics::GraphicsContext &context) -> Error;
};

extern const Graphics::BufferFormat CameraBufferFormat;

static const Type CameraType = Type("Camera");

struct LuaCamera : LuaWrap::LuaECSObject {
  explicit LuaCamera(flecs::entity entity) : LuaECSObject(entity) {}

  static auto Create(lua_State *state) -> int;

  static auto GetType() -> const Type * { return &CameraType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override {
    return LuaCamera::GetType();
  }

  static auto Render(lua_State *state) -> int;

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

  static auto GetBuffer(lua_State *state) -> int;
  static auto GetRendertarget(lua_State *state) -> int;
};

auto GetLuaCameraClass() -> ::LuaWrap::LuaClass;

} // namespace Engine