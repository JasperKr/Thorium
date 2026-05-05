#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/texture.hpp"
#include "Modules/color.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "material.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

struct Renderer {
  Material DefaultMaterial;

  auto Initialize(Graphics::GraphicsContext &context) -> Error {
    constexpr uint32_t CheckerboardTextureSize = 8;
    constexpr Color CheckerboardColorPrimary = {0.0F, 0.0F, 0.0F, 1.0F};
    constexpr Color CheckerboardColorSecondary = {1.0F, 0.0F, 1.0F, 1.0F};

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
    DefaultMaterial.cullMode = VK_CULL_MODE_NONE;
    DefaultMaterial.alphaMode = AlphaMode::Opaque;

    return {};
  }

  void Deinitialize() { DefaultMaterial = Material(); }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Renderer RendererInstance;

} // namespace Engine::Renderer