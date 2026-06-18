#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace SnapEngine::Renderer {
struct LightprobePrefilterManager {
  constexpr static uint32_t PrefilteredRadianceMapSize = 128;
  constexpr static uint32_t MaxCubemaps = 64;
  constexpr static uint32_t IblRoughnessOneLevel = 6;
  constexpr static uint32_t ProbeReflectionMipLevels = 7;

  auto Initialize(const Graphics::GraphicsContext &context) -> Error {
    PrefilteredRadianceCoeffs = CHECK_RES(
        Filesystem::ReadFile("Scripting/Graphics/Assets/coeffs_quad_32.bin"));

    SceneCubemap = CHECK_RES(Graphics::Create(
        context,
        {
            .size = {PrefilteredRadianceMapSize, PrefilteredRadianceMapSize, 1},
            .arrayLayers = 6,
            .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                     VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .mipmapCount = ProbeReflectionMipLevels,
        }));

    constexpr static float EnvironmentMapLevel0PB = 6.0F;
    constexpr static float EnvironmentMapLevel1PB = 0.0F;

    for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
      uint32_t levelSize = PrefilteredRadianceMapSize >> level;

      float glossiness = (EnvironmentMapLevel0PB - static_cast<float>(level)) /
                         (EnvironmentMapLevel0PB - EnvironmentMapLevel1PB);

      levelParameters.at(level) = {
          .Level = level,
          .Glossiness = glossiness,
          .Roughness = GlossinessToRoughness(glossiness),
          .Resolution = levelSize,
      };
    }

    return {};
  }
  auto Deinitialize() -> void;

private:
  struct LevelParameters {
    uint32_t Level;
    float Glossiness;
    float Roughness;
    uint32_t Resolution;
  };

  std::array<LevelParameters, ProbeReflectionMipLevels> levelParameters{};
  Ref<Graphics::Texture> SceneCubemap;
  std::vector<uint8_t> PrefilteredRadianceCoeffs;

  static auto GlossinessToRoughness(float glossiness) -> float {
    constexpr float GGX_MAX_SPECULAR_POWER = 18.0F;

    // NOLINTBEGIN
    float exponent = std::pow(2.0F, glossiness * GGX_MAX_SPECULAR_POWER);

    return std::pow(2.0F / (1.0F + exponent), 0.25F);
    // NOLINTEND
  }
};
} // namespace SnapEngine::Renderer