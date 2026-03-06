#pragma once

#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include <array>
#include <string>

#include <vulkan/vulkan_core.h>
namespace Renderer {

enum class AlphaMode : uint8_t {
  Opaque = 0,
  Mask = 1,
  Blend = 2,
};

using TexRef = Ref<Graphics::Texture::Texture>;

struct Material {
  std::string Name;

  VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
  AlphaMode AlphaModeSetting = AlphaMode::Opaque;
  float AlphaCutoff = 0.5F; // NOLINT
  Ref<Graphics::Shader::ShaderModule> Shader;

  TexRef Preview;

  TexRef AlbedoTexture;            // Linear RGBA
  TexRef NormalTexture;            // Linear RGB
  TexRef MetallicRoughnessTexture; // Linear RG
  TexRef AmbientOcclusionTexture;  // Linear R
  TexRef ReflectanceTexture;       // Linear R
  TexRef EmissiveTexture;          // Linear RGB

  Math::Vec4 AlbedoFactor = Math::Vec4(1.0F, 1.0F, 1.0F, 1.0F);
  float RoughnessFactor = 1.0F;
  float MetallicFactor = 1.0F;
  float ReflectanceFactor = 1.0F;
  Math::Vec3 EmissiveFactor = Math::Vec3(0.0F, 0.0F, 0.0F);

  // which UV set each texture uses in the order of:
  // 0: Albedo
  // 1: Normal
  // 2: Roughness
  // 3: Metallic
  // 4: AO
  // 5: Reflectance
  // 6: Emissive
  // NOLINTNEXTLINE (magic numbers)
  std::array<uint8_t, 7> TextureUVIndices = {0};
};
} // namespace Renderer