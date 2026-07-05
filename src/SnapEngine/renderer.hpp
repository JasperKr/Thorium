#pragma once

#include "Editor/lineDrawer.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Renderer/prefilterManager.hpp"
#include "material.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

enum class ShaderKey : uint8_t {
  DepthPrepass,
  DepthMaskPass,
  Deferred,
  SimpleLighting,
  PostProcessing,
  Skybox,
  FillSkybox,
  TransparencyForward,
  ApplyEnvironmentMap,
};

struct ShaderConfiguration {
  std::string path;
  std::string name;
  std::vector<Graphics::ShaderExtern> Externs;
};

const std::unordered_map<ShaderKey, ShaderConfiguration> ShaderConfigurations =
    {
        {ShaderKey::DepthPrepass,
         {.path = "Scripting/Graphics/Shaders/Geometry/depthPrepass.slang"}},
        {ShaderKey::DepthMaskPass,
         {.path = "Scripting/Graphics/Shaders/Geometry/depthMaskPass.slang"}},
        {ShaderKey::Deferred,
         {.path = "Scripting/Graphics/Shaders/Geometry/deferred.slang"}},
        {ShaderKey::SimpleLighting,
         {.path = "Scripting/Graphics/Shaders/Lighting/simple.slang"}},
        {ShaderKey::PostProcessing,
         {.path = "Scripting/Graphics/Shaders/PostProcessing/"
                  "postProcessing.slang"}},
        {ShaderKey::Skybox,
         {.path = "Scripting/Graphics/Shaders/PostProcessing/skybox.slang"}},
        {ShaderKey::FillSkybox,
         {.path =
              "Scripting/Graphics/Shaders/PostProcessing/fillSkybox.slang"}},
        {ShaderKey::TransparencyForward,
         {.path =
              "Scripting/Graphics/Shaders/Geometry/transparencyForward.slang"}},
        {ShaderKey::ApplyEnvironmentMap,
         {.path = "Scripting/Graphics/Shaders/IBL/sampleEnvMaps.slang"}},
};

const std::vector<Graphics::BufferComponent> MaterialBufferComponents = {
    Graphics::BufferComponent{
        .name = "AlbedoFactor",
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "RoughnessFactor",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "MetallicFactor",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "EmissiveFactor",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "AlphaMode",
        .format = VK_FORMAT_R32_UINT,
    },
    Graphics::BufferComponent{
        .name = "AlphaCutoff",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "TextureUVIndices",
        .format = VK_FORMAT_R32G32_UINT,
    },
};

const std::vector<Graphics::BufferComponent> ModelTransformBufferComponents = {
    Graphics::BufferComponent{
        .name = "ModelMatrix",
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .arraySize = 4,
        .isMatrix = true,
    },
    Graphics::BufferComponent{
        .name = "NormalMatrix",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .arraySize = 3,
        .isMatrix = true,
    },
};

/*
struct LightProbe {
  float3 position;
  int index;

  // Sphere
  // same as above, but in a sphere
  float radius;
  float innerRadius;
};
*/

const std::vector<Graphics::BufferComponent> LightProbeBufferComponents = {
    Graphics::BufferComponent{
        .name = "Position",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Index",
        .format = VK_FORMAT_R32_UINT,
    },
    Graphics::BufferComponent{
        .name = "Radius",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "InnerRadius",
        .format = VK_FORMAT_R32_SFLOAT,
    },
};

const Graphics::BufferFormat LightProbeBufferFormat{LightProbeBufferComponents};

struct Renderer {
  struct Lights {
    Ref<Graphics::StructuredBuffer> PointLightsBuffer;
    Ref<Graphics::StructuredBuffer> SpotLightsBuffer;
    Ref<Graphics::StructuredBuffer> RectangleLightsBuffer;
    Ref<Graphics::StructuredBuffer> SphereLightsBuffer;
    Ref<Graphics::StructuredBuffer> DirectionalLightsBuffer;

    std::vector<uint8_t> PointLightData;
    std::vector<uint8_t> SpotLightData;
    std::vector<uint8_t> RectangleLightData;
    std::vector<uint8_t> SphereLightData;
    std::vector<uint8_t> DirectionalLightData;

    int PointLightCount = 0;
    int SpotLightCount = 0;
    int RectangleLightCount = 0;
    int SphereLightCount = 0;
    int DirectionalLightCount = 0;
  };

  auto Initialize(Graphics::GraphicsContext &context) -> Error {
    constexpr size_t InitialMaterialBufferSize = 1024UL;
    constexpr size_t InitialModelTransformBufferSize = 4096UL;

    CHECK_ERR(InitializeDefaultMaterial(context));

    CHECK_ERR(InitializeMaterialBuffer(context, InitialMaterialBufferSize));
    CHECK_ERR(InitializeModelTransformsBuffer(context,
                                              InitialModelTransformBufferSize));

    CHECK_ERR(InitializeLightBuffers(context));
    CHECK_ERR(PrefilterManager.Initialize(context));

    LightProbeBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(
        context, LightProbeBufferFormat, 64UL,
        Graphics::StructuredBufferCreationInfo{
            .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .usageFlags =
                static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
                static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
            .debugName = "Light Probe Buffer",
        }));

    CHECK_ERR(LineDrawer.Initialize(context));

    initialized = true;
    return {};
  }

  void Deinitialize();

  auto GetNewMaterialIndex() -> Result<size_t>;

  auto BindLightBuffers(const Graphics::GraphicsContext &context,
                        const Ref<Graphics::Shader> &shader) const -> Error;

  auto GetShader(ShaderKey shaderKey) -> Result<Ref<Graphics::Shader>>;

  auto GetDefaultMaterial() const -> const Material & {
    return DefaultMaterial;
  }
  auto GetNoMaterial() const -> const Material & { return NoMaterial; }
  auto GetMaterialsBuffer() const -> Ref<Graphics::StructuredBuffer> {
    return MaterialsBuffer;
  }
  auto GetModelTransformsBuffer() const -> Ref<Graphics::StructuredBuffer> {
    return ModelTransformsBuffer;
  }
  auto AssureModelTransformBufferSize(size_t minimumSize) -> Error;

  auto GetSceneLightBuffers() -> Lights & { return SceneLightBuffers; }
  auto GetSceneLightBuffers() const -> const Lights & {
    return SceneLightBuffers;
  }

  auto GetPrefilterManager() -> LightprobePrefilterManager & {
    return PrefilterManager;
  }
  auto GetPrefilterManager() const -> const LightprobePrefilterManager & {
    return PrefilterManager;
  }

  auto NewFrame() -> void { ModelTransformBufferElementCount = 0; }

  auto GetLightProbeBuffer() -> Ref<Graphics::StructuredBuffer> {
    return LightProbeBuffer;
  }

  auto GetLineDrawer() -> LineDrawer & { return LineDrawer; }

private:
  std::unordered_map<ShaderKey, Ref<Graphics::Shader>> LoadedShaders;
  Material NoMaterial;
  Material DefaultMaterial;

  Ref<Graphics::StructuredBuffer> MaterialsBuffer;
  std::unordered_set<size_t> UsedMaterialIndices;

  Ref<Graphics::StructuredBuffer> ModelTransformsBuffer;
  size_t ModelTransformBufferElementCount = 0;

  Ref<Graphics::StructuredBuffer> LightProbeBuffer;

  Lights SceneLightBuffers;
  LightprobePrefilterManager PrefilterManager;

  LineDrawer LineDrawer;

  bool initialized = false;

  auto InitializeMaterialBuffer(Graphics::GraphicsContext &context,
                                size_t initialSize) -> Error;
  auto InitializeLightBuffers(Graphics::GraphicsContext &context) -> Error;
  auto InitializeModelTransformsBuffer(Graphics::GraphicsContext &context,
                                       size_t initialSize) -> Error;

  auto InitializeDefaultMaterial(Graphics::GraphicsContext &context) -> Error;
};

auto DrawFullScreen(const Graphics::GraphicsContext &context) -> Error;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Renderer RendererInstance;

} // namespace Engine::Renderer