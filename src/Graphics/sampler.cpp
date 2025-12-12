#include "sampler.hpp"
#include "Modules/console.hpp"
#include <iostream>
#include <unordered_map>

namespace Graphics::Texture {

auto GetOrCreateSampler(GraphicsContext &context,
                        const SamplerDescription &description) -> VkSampler {
  static std::unordered_map<SamplerDescription, VkSampler, SamplerDescHash>
      samplerCache;
  samplerCache = {};

  auto sampler = samplerCache.find(description);
  if (sampler != samplerCache.end()) {
    return sampler->second; // Return cached sampler
  }

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = description.magFilter;
  samplerInfo.minFilter = description.minFilter;
  samplerInfo.mipmapMode = description.mipmapMode;
  samplerInfo.addressModeU = description.addressModeU;
  samplerInfo.addressModeV = description.addressModeV;
  samplerInfo.addressModeW = description.addressModeW;
  samplerInfo.mipLodBias = description.mipLodBias;
  samplerInfo.anisotropyEnable =
      description.anisotropyEnable ? VK_TRUE : VK_FALSE;
  samplerInfo.maxAnisotropy = description.maxAnisotropy;
  samplerInfo.compareEnable = description.compareEnable ? VK_TRUE : VK_FALSE;
  samplerInfo.compareOp = description.compareOp;
  samplerInfo.minLod = description.minLod;
  samplerInfo.maxLod = description.maxLod;
  samplerInfo.borderColor = description.borderColor;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;

  VkSampler vkSampler = VK_NULL_HANDLE;
  VkResult result =
      vkCreateSampler(context.device, &samplerInfo, nullptr, &vkSampler);

  if (result != VK_SUCCESS) {
    PrintError("Failed to create sampler: {}\n", Error::Create(result).message);
    return VK_NULL_HANDLE; // Failed to create sampler
  }

  samplerCache[description] = vkSampler;
  return vkSampler;
}

} // namespace Graphics::Texture
