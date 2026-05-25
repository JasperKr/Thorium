#pragma once

#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
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
};

struct ShaderConfiguration {
  std::string path;
  std::string name;
  std::vector<Graphics::Shader::ShaderExtern> Externs;
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

    CHECK_ERR(InitializeDefaultMaterial(context));

    CHECK_ERR(InitializeMaterialBuffer(context, InitialMaterialBufferSize));

    CHECK_ERR(InitializeLightBuffers(context));

    initialized = true;
    return {};
  }

  void Deinitialize() {
    if (!initialized) {
      return;
    }

    NoMaterial = Material();
    MaterialsBuffer.reset();
  }

  auto ResizeMaterialBuffer(Graphics::GraphicsContext &context, size_t newSize)
      -> Error;

  auto GetNewMaterialIndex() -> Result<size_t>;

  auto BindLightBuffers(const Graphics::GraphicsContext &context,
                        const Ref<Graphics::Shader::ShaderModule> &shader) const
      -> Error;

  auto GetShader(ShaderKey shaderKey)
      -> Result<Ref<Graphics::Shader::ShaderModule>>;

  auto GetDefaultMaterial() const -> const Material & {
    return DefaultMaterial;
  }
  auto GetNoMaterial() const -> const Material & { return NoMaterial; }
  auto GetMaterialsBuffer() const -> Ref<Graphics::StructuredBuffer> {
    return MaterialsBuffer;
  }
  auto GetSceneLightBuffers() -> Lights & { return SceneLightBuffers; }
  auto GetSceneLightBuffers() const -> const Lights & {
    return SceneLightBuffers;
  }

private:
  std::unordered_map<ShaderKey, Ref<Graphics::Shader::ShaderModule>>
      LoadedShaders;
  Material NoMaterial;
  Material DefaultMaterial;
  Ref<Graphics::StructuredBuffer> MaterialsBuffer;
  Lights SceneLightBuffers;
  std::unordered_set<size_t> UsedMaterialIndices;
  bool initialized = false;

  auto InitializeMaterialBuffer(Graphics::GraphicsContext &context,
                                size_t initialSize) -> Error;

  auto InitializeDefaultMaterial(Graphics::GraphicsContext &context) -> Error;

  auto InitializeLightBuffers(Graphics::GraphicsContext &context) -> Error;
};

auto DrawFullScreen(const Graphics::GraphicsContext &context) -> Error;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Renderer RendererInstance;

} // namespace Engine::Renderer