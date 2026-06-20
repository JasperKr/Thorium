#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Renderer/lightProbe.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace SnapEngine::Renderer {
struct LightprobePrefilterManager {
  // cubemap size
  constexpr static uint32_t PrefilteredRadianceCubeMapSize = 128;

  // Octahedral size
  constexpr static uint32_t PrefilteredRadianceMapsSize = 512;
  constexpr static uint32_t PrefilteredIrradianceMapsSize = 32;

  constexpr static uint32_t MaxEnvMaps = 512;
  constexpr static uint32_t EnvMapReallocationStep = 64;

  constexpr static uint32_t IblRoughnessOneLevel = 6;
  constexpr static uint32_t ProbeReflectionMipLevels = 7;

  auto Initialize(const Graphics::GraphicsContext &context) -> Error {
    PrefilteredRadianceCoeffs = CHECK_RES(
        Filesystem::ReadFile("Scripting/Graphics/Assets/coeffs_quad_32.bin"));

    SceneCubemap = CHECK_RES(Graphics::Texture::Create(
        context,
        {
            .size = {PrefilteredRadianceCubeMapSize,
                     PrefilteredRadianceCubeMapSize, 1},
            .arrayLayers = 6,
            .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                     VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .mipmapCount = ProbeReflectionMipLevels,
            .textureType = Graphics::TextureType::CUBEMAP,
        }));

    TemporaryRadianceMap = CHECK_RES(Graphics::Texture::Create(
        context,
        {
            .size = {PrefilteredRadianceCubeMapSize,
                     PrefilteredRadianceCubeMapSize, 1},
            .arrayLayers = 6,
            .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                     VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .mipmapCount = ProbeReflectionMipLevels,
            .textureType = Graphics::TextureType::CUBEMAP,
        }));

    TemporaryRadianceWriteTexture = CHECK_RES(Graphics::Texture::Create(
        context,
        {
            .size = {PrefilteredRadianceCubeMapSize,
                     PrefilteredRadianceCubeMapSize, 1},
            .arrayLayers = 6,
            .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                     VK_IMAGE_USAGE_STORAGE_BIT,
            .mipmapCount = ProbeReflectionMipLevels,
            .textureType = Graphics::TextureType::CUBEMAP,
        }));

    constexpr static float EnvironmentMapLevel0PB = 6.0F;
    constexpr static float EnvironmentMapLevel1PB = 0.0F;

    for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
      uint32_t levelSize = PrefilteredRadianceCubeMapSize >> level;

      float glossiness = (EnvironmentMapLevel0PB - static_cast<float>(level)) /
                         (EnvironmentMapLevel0PB - EnvironmentMapLevel1PB);

      levelParameters.at(level) = {
          .Level = level,
          .Glossiness = glossiness,
          .Roughness = GlossinessToRoughness(glossiness),
          .Resolution = levelSize,
      };
    }

    CHECK_ERR(CreateStorageTextures(context, EnvMapReallocationStep));

    DownsampleShader = CHECK_RES(Graphics::Shader::ShaderModule::Create(
        context, "Scripting/Graphics/Shaders/IBL/downsample.slang",
        "Environment map downsample"));

    StoreEnvironmentMapShader =
        CHECK_RES(Graphics::Shader::ShaderModule::Create(
            context, "Scripting/Graphics/Shaders/IBL/envToOct.slang",
            "Store environment map"));

    PrefilterRadianceShader = CHECK_RES(Graphics::Shader::ShaderModule::Create(
        context, "Scripting/Graphics/Shaders/IBL/filterRadiance.slang",
        "Prefilter radiance"));

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

  Ref<Graphics::Texture> RadianceMaps;
  Ref<Graphics::Texture> IrradianceMaps;

  Ref<Graphics::Texture> TemporaryRadianceMap;
  Ref<Graphics::Texture> TemporaryRadianceWriteTexture;

  std::vector<uint8_t> PrefilteredRadianceCoeffs;
  std::array<bool, MaxEnvMaps> EnvMapUsed{};

  Ref<Graphics::Shader::ShaderModule> DownsampleShader;
  Ref<Graphics::Shader::ShaderModule> PrefilterRadianceShader;
  Ref<Graphics::Shader::ShaderModule> PrefilterIrradianceShader;
  Ref<Graphics::Shader::ShaderModule> StoreEnvironmentMapShader;

  static auto GlossinessToRoughness(float glossiness) -> float {
    constexpr float GGX_MAX_SPECULAR_POWER = 18.0F;

    // NOLINTBEGIN
    float exponent = std::pow(2.0F, glossiness * GGX_MAX_SPECULAR_POWER);

    return std::pow(2.0F / (1.0F + exponent), 0.25F);
    // NOLINTEND
  }

  auto CreateStorageTextures(const Graphics::GraphicsContext &context,
                             uint32_t arrayLayers) -> Error {
    RadianceMaps = CHECK_RES(Graphics::Texture::Create(
        context,
        {
            .size = {PrefilteredRadianceMapsSize, PrefilteredRadianceMapsSize,
                     1},
            .arrayLayers = arrayLayers,
            .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                     VK_IMAGE_USAGE_STORAGE_BIT,
            .mipmapCount = ProbeReflectionMipLevels,
        }));

    IrradianceMaps = CHECK_RES(Graphics::Texture::Create(
        context,
        {
            .size = {PrefilteredIrradianceMapsSize,
                     PrefilteredIrradianceMapsSize, 1},
            .arrayLayers = arrayLayers,
            .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                     VK_IMAGE_USAGE_STORAGE_BIT,
            .mipmapCount = 1,
        }));

    return {};
  }

  auto GetFreeEnvMapIndex() -> std::optional<int32_t> {
    for (int32_t i = 0; i < MaxEnvMaps; i++) {
      if (!EnvMapUsed.at(i)) {
        return i;
      }
    }

    return {};
  }

  auto FreeEnvMapIndex(int32_t index) -> void {
    if (index >= MaxEnvMaps) {
      return;
    }

    EnvMapUsed.at(index) = false;
  }

  auto PrefilterRadianceMap(const Graphics::GraphicsContext &context,
                            const Ref<Graphics::Texture> &envMap,
                            const Ref<Graphics::Texture> &output) -> Error {

    return {};
  }

  auto PrefilterEnvironmentMap(const Graphics::GraphicsContext &context,
                               const Ref<Graphics::Texture> &envMap,
                               LightProbe &lightProbe) -> Error {
    if (lightProbe.EnvironmentMapIndex < 0) {
      return Error::Create("Invalid environment map index");
    }

    if (static_cast<uint32_t>(lightProbe.EnvironmentMapIndex) >= MaxEnvMaps) {
      return Error::Create("Environment map index out of bounds");
    }

    if (!EnvMapUsed.at(lightProbe.EnvironmentMapIndex)) {
      return Error::Create("Environment map index not in use");
    }

    return {};
  }
};
} // namespace SnapEngine::Renderer