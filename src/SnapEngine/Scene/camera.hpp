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
#include "Scene/cameraMatrices.hpp"
#include "Scene/environment.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <cstdint>
#include <vector>
namespace Engine {

struct DrawData {
  Transform Transform;
  CameraMatrices Matrices;
};

enum class AgxLook : uint8_t {
  Default,
  Punchy,
  Golden,
};

struct Camera {
  friend struct LuaCamera;

  struct Settings {
    bool DoPostProcessing = true;
  };

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

  void SetSettings(const Settings &newSettings) { settings = newSettings; }

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

  [[nodiscard]] auto GetDimensions() const -> Math::Uvec2 { return Dimensions; }

  auto SetDimensions(Math::Uvec2 newDimensions) -> void {
    if (Dimensions == newDimensions) {
      return;
    }

    Dimensions = newDimensions;
    projectionDirty = true;
    ConfigureRendertargets();
    AspectRatio = static_cast<Math::Scalar>(Dimensions.x) /
                  static_cast<Math::Scalar>(Dimensions.y);
  }

  static void RegisterCameraSystems(Scene &scene);

  static auto Create(const Graphics::GraphicsContext &context,
                     Math::Scalar verticalFOVDeg, Math::Uvec2 Dimensions,
                     Math::Scalar near, Math::Scalar far) -> Result<Camera>;

  auto WriteToBuffer(const CameraMatrices &cameraMatrices,
                     const Transform &transform) const -> Error;

  auto Resize(Math::Uvec2 newDimensions) -> void {
    Dimensions = newDimensions;
    projectionDirty = true;
  }

  auto Render(const Graphics::GraphicsContext &context,
              const DrawData &drawData, Scene *scene) -> Error;

  auto RenderSkyboxOnly(const Graphics::GraphicsContext &context,
                        const Environment &environment) -> Error;

  // Descriptors for the textures we'd like to own.
  struct CameraRendertargets {
    // Depth texture
    Renderer::RendertargetDescriptor Depth;

    // Normals, encoded as a2r10g10b10_unorm, should be encoded from [-1, 1] to [0, 1] when writing and [0, 1] to [-1, 1] when reading
    Renderer::RendertargetDescriptor Normal;

    // Albedo, encoded as a2r10g10b10_unorm
    Renderer::RendertargetDescriptor Albedo;

    // Materials, r8g8b8a8_unorm, where R = metallic, G = perceptual roughness, B = reflectance, A = texture ao
    Renderer::RendertargetDescriptor Material;

    // Emissive, encoded as b10g11r11_ufloat
    Renderer::RendertargetDescriptor Emissive;

    // Motion vectors, encoded as rg16_snorm
    Renderer::RendertargetDescriptor Motion;

    // Final Incoming light, encoded as b10g11r11_ufloat
    Renderer::RendertargetDescriptor IncomingLight;

    // Direct illumination, encoded as b10g11r11_ufloat
    Renderer::RendertargetDescriptor DirectLighting;

    // Diffuse irradiance, encoded as b10g11r11_ufloat
    Renderer::RendertargetDescriptor Irradiance;

    // Post-processed output, encoded as a2r10g10b10_unorm
    Renderer::RendertargetDescriptor PostProcessed;

    // Bloom-Downsample Chain, encoded as b10g11r11_ufloat
    Renderer::RendertargetDescriptor BloomDownsampleChain;

    // Shadow visibility array texture, encoded as r8_unorm
    Renderer::RendertargetDescriptor ShadowVisibility;
  };

  // References to the textures we currently own. Dynamic
  struct AllocatedTextures {
    Ref<Graphics::Texture> Depth;
    Ref<Graphics::Texture> Normal;
    Ref<Graphics::Texture> Albedo;
    Ref<Graphics::Texture> Material;
    Ref<Graphics::Texture> Emissive;
    Ref<Graphics::Texture> Motion;
    Ref<Graphics::Texture> IncomingLight;
    Ref<Graphics::Texture> DirectLighting;
    Ref<Graphics::Texture> Irradiance;
    Ref<Graphics::Texture> PostProcessed;
    Ref<Graphics::Texture> BloomDownsampleChain;
    Ref<Graphics::Texture> ShadowVisibility;
    Ref<Graphics::Texture> PreviousShadowVisibility;

    void Reset() {
      Depth = nullptr;
      Normal = nullptr;
      Albedo = nullptr;
      Material = nullptr;
      Emissive = nullptr;
      Motion = nullptr;
      IncomingLight = nullptr;
      DirectLighting = nullptr;
      Irradiance = nullptr;
      PostProcessed = nullptr;
      BloomDownsampleChain = nullptr;
      ShadowVisibility = nullptr;
      PreviousShadowVisibility = nullptr;
    }
  };

  struct PersistentTextureSettings {
    bool Depth = false;
    bool Normal = false;
    bool Albedo = false;
    bool Material = false;
    bool Emissive = false;
    bool Motion = false;
    bool IncomingLight = false;
    bool DirectLighting = false;
    bool Irradiance = false;
    bool PostProcessed = false;
    bool BloomDownsampleChain = false;
    bool ShadowVisibility = true;
  };

  // NOLINTBEGIN
  struct PostProcessingConfig {
    float Temperature = 0.0F;
    float Tint = 0.0F;
    bool ApplyAGX = true;
    float Contrast = 1.0F;
    float Saturation = 1.0F;
    float Vignette = 0.0F;
    float Exposure = 0.2F;
    AgxLook Look = AgxLook::Default;
  };
  // NOLINTEND

  auto SetPostProcessingConfig(const PostProcessingConfig &config) -> void {
    postProcessingConfig = config;
  }

  void
  SetPersistentTextureSettings(const PersistentTextureSettings &newSettings) {
    persistentTextureSettings = newSettings;
  }

  [[nodiscard]] auto GetPostProcessingConfig() const -> PostProcessingConfig {
    return postProcessingConfig;
  }

  [[nodiscard]] auto GetPostProcessingConfig() -> PostProcessingConfig & {
    return postProcessingConfig;
  }

  [[nodiscard]] auto GetOwnedTextures() const -> const AllocatedTextures & {
    return OwnedTextures;
  }

  [[nodiscard]] auto GetOwnedTextures() -> AllocatedTextures & {
    return OwnedTextures;
  }

  [[nodiscard]] auto GetRendertargets() const -> const CameraRendertargets & {
    return Rendertargets;
  }

  [[nodiscard]] auto GetSettings() const -> Settings { return settings; }

  [[nodiscard]] auto GetPersistentTextureSettings() const
      -> PersistentTextureSettings {
    return persistentTextureSettings;
  }

  auto DrawGUI(flecs::entity entity) -> void;

private:
  Settings settings;
  PersistentTextureSettings persistentTextureSettings;

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

  int VisibleProbeCount = 0;

  bool projectionDirty = true;

  auto ConfigureRendertargets() -> void;

  auto ApplyPostProcessing(const Graphics::GraphicsContext &context) -> Error;

  auto RenderSkybox(const Graphics::GraphicsContext &context,
                    const Environment &environment) -> Error;

  auto FillSkybox(const Graphics::GraphicsContext &context,
                  const Environment &environment) -> Error;

  auto ApplyLightProbes(const Graphics::GraphicsContext &context,
                        const DrawData &drawData, Scene *scene) -> Error;

  auto UpdateClosestLightProbes(int max, std::vector<uint8_t> &data,
                                const DrawData &drawData, Scene *scene) -> void;
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

  static auto GetDimensions(lua_State *state) -> int;
  static auto SetDimensions(lua_State *state) -> int;

  static auto GetWidth(lua_State *state) -> int;
  static auto SetWidth(lua_State *state) -> int;

  static auto GetHeight(lua_State *state) -> int;
  static auto SetHeight(lua_State *state) -> int;

  static auto GetPersistentTextureSettings(lua_State *state) -> int;
  static auto SetPersistentTextureSettings(lua_State *state) -> int;
};

auto GetLuaCameraClass() -> ::LuaWrap::LuaClass;

} // namespace Engine