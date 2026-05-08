#pragma once

#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/texture.hpp"
#include "Modules/color.hpp"
#include "Modules/error.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "material.hpp"
#include <cstddef>
#include <cstdint>
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
  Material DefaultMaterial;
  Ref<Graphics::StructuredBuffer> MaterialsBuffer;
  std::unordered_set<size_t> UsedMaterialIndices;
  bool initialized = false;

  auto Initialize(Graphics::GraphicsContext &context) -> Error {
    constexpr uint32_t CheckerboardTextureSize = 8;
    constexpr Color CheckerboardColorPrimary = {0.0F, 0.0F, 0.0F, 1.0F};
    constexpr Color CheckerboardColorSecondary = {1.0F, 0.0F, 1.0F, 1.0F};
    constexpr size_t InitialMaterialBufferSize = 1024UL;

    Image::ImageData defaultTextureData(CheckerboardTextureSize,
                                        CheckerboardTextureSize,
                                        VK_FORMAT_R8G8B8A8_UNORM);

    for (uint32_t column = 0; column < CheckerboardTextureSize; ++column) {
      for (uint32_t row = 0; row < CheckerboardTextureSize; ++row) {
        bool isPrimaryColor = (row % 2 == 0 && column % 2 == 0) ||
                              (row % 2 == 1 && column % 2 == 1);

        Color pixelColor = isPrimaryColor ? CheckerboardColorPrimary
                                          : CheckerboardColorSecondary;

        auto error = defaultTextureData.SetColor({column, row}, pixelColor);
        if (Error::IsError(error)) {
          return error;
        }
      }
    }

    auto defaultTextureResult = Graphics::LoadFromMemory(
        context, defaultTextureData, VK_IMAGE_USAGE_SAMPLED_BIT);

    if (Error::IsError(defaultTextureResult)) {
      return defaultTextureResult.error();
    }

    DefaultMaterial = Material();

    DefaultMaterial.name = "Default Material";
    DefaultMaterial.albedoTexture = defaultTextureResult.value();
    DefaultMaterial.albedoTexture->SetFilter(
        VK_FILTER_LINEAR, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
    DefaultMaterial.cullMode = VK_CULL_MODE_NONE;
    DefaultMaterial.alphaMode = AlphaMode::Opaque;

    const Graphics::StructuredBufferCreationInfo materialBufferCreateInfo{
        .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .debugName = "Material Buffer",
    };

    auto format = Graphics::BufferFormat(MaterialBufferComponents);

    auto materialBufferResult = Graphics::StructuredBuffer::Create(
        context, format, InitialMaterialBufferSize, materialBufferCreateInfo);

    if (Error::IsError(materialBufferResult)) {
      return materialBufferResult.error();
    }

    MaterialsBuffer = materialBufferResult.value();

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
      -> Error {
    auto previousSize = MaterialsBuffer->GetElementCount();
    if (newSize <= previousSize) {
      return {};
    }

    auto format = Graphics::BufferFormat(MaterialBufferComponents);

    auto newBufferResult = Graphics::StructuredBuffer::Create(
        *Graphics::GetCurrentGraphicsContext(), format, newSize,
        Graphics::StructuredBufferCreationInfo{
            .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .debugName = "Material Buffer",
        });

    if (Error::IsError(newBufferResult)) {
      return newBufferResult.error();
    }

    auto newBuffer = newBufferResult.value();

    auto copyError = MaterialsBuffer->GetBuffer()->CopyTo(
        context, *newBuffer->GetBuffer(), 0, 0, MaterialsBuffer->GetSize());
    if (Error::IsError(copyError)) {
      return copyError;
    }

    MaterialsBuffer = std::move(newBuffer);

    return {};
  }

  auto GetNewMaterialIndex() -> Result<size_t> {
    size_t newIndex = 0;
    while (UsedMaterialIndices.contains(newIndex)) {
      newIndex++;
    }
    UsedMaterialIndices.insert(newIndex);

    if (newIndex >= MaterialsBuffer->GetElementCount()) {
      auto newSize = MaterialsBuffer->GetElementCount() * 2;

      auto resizeResult =
          ResizeMaterialBuffer(*Graphics::GetCurrentGraphicsContext(), newSize);
      if (Error::IsError(resizeResult)) {
        return resizeResult.AsUnexpected();
      }
    }

    return newIndex;
  }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Renderer RendererInstance;

} // namespace Engine::Renderer