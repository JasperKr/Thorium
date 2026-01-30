#include "sampler.hpp"
#include "Modules/console.hpp"
#include <unordered_map>

namespace Graphics::Texture {

std::unordered_map<SamplerDescription, VkSampler, SamplerDescHash>
    SamplerCache; // NOLINT

auto GetOrCreateSampler(GraphicsContext &context,
                        const SamplerDescription &description) -> VkSampler {

  auto sampler = SamplerCache.find(description);
  if (sampler != SamplerCache.end()) {
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
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    VkResult result =
        vkCreateSampler(context.device, &samplerInfo, nullptr, &vkSampler);

    if (result != VK_SUCCESS) {
      PrintError("Failed to create sampler: {}\n",
                 Error::Create(result).message);
      return VK_NULL_HANDLE; // Failed to create sampler
    }
  }

  SamplerCache[description] = vkSampler;
  return vkSampler;
}

auto DestroySamplers(GraphicsContext &context) -> void {
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  for (auto &pair : SamplerCache) {
    vkDestroySampler(context.device, pair.second, nullptr);
  }
  SamplerCache.clear();
}

} // namespace Graphics::Texture
