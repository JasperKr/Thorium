#pragma once

#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "material.hpp"
#include <cstddef>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

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
  };

  Material DefaultMaterial;
  Ref<Graphics::StructuredBuffer> MaterialsBuffer;
  Lights SceneLightBuffers;
  std::unordered_set<size_t> UsedMaterialIndices;
  bool initialized = false;

  auto Initialize(Graphics::GraphicsContext &context) -> Error {
    constexpr size_t InitialMaterialBufferSize = 1024UL;

    auto error = InitializeDefaultMaterial(context);
    if (Error::IsError(error)) {
      return error;
    }

    error = InitializeMaterialBuffer(context, InitialMaterialBufferSize);
    if (Error::IsError(error)) {
      return error;
    }

    error = InitializeLightBuffers(context);
    if (Error::IsError(error)) {
      return error;
    }

    initialized = true;
    return {};
  }

  void Deinitialize() {
    if (!initialized) {
      return;
    }

    DefaultMaterial = Material();
    MaterialsBuffer.reset();
  }

  auto ResizeMaterialBuffer(Graphics::GraphicsContext &context, size_t newSize)
      -> Error;

  auto GetNewMaterialIndex() -> Result<size_t>;

private:
  auto InitializeMaterialBuffer(Graphics::GraphicsContext &context,
                                size_t initialSize) -> Error;

  auto InitializeDefaultMaterial(Graphics::GraphicsContext &context) -> Error;

  auto InitializeLightBuffers(Graphics::GraphicsContext &context) -> Error;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Renderer RendererInstance;

} // namespace Engine::Renderer