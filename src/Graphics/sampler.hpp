#pragma once

#include "Graphics/graphics.hpp"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics::Texture {

struct SamplerDescription {
  VkFilter magFilter;
  VkFilter minFilter;
  VkSamplerMipmapMode mipmapMode;
  VkSamplerAddressMode addressModeU;
  VkSamplerAddressMode addressModeV;
  VkSamplerAddressMode addressModeW;
  float mipLodBias;
  bool anisotropyEnable;
  float maxAnisotropy;
  bool compareEnable;
  VkCompareOp compareOp;
  float minLod;
  float maxLod;
  VkBorderColor borderColor;

  auto operator==(const SamplerDescription &other) const -> bool = default;
};

auto inline Hash(size_t seed, size_t value) -> size_t {
  constexpr size_t offset = 0x9e3779b9;
  constexpr size_t lshift = 6;
  constexpr size_t rshift = 2;

  return seed ^ (value + offset + (seed << lshift) + (seed >> rshift));
}

struct SamplerDescHash {
  auto operator()(const SamplerDescription &desc) const -> size_t {
    size_t hash = 0;

    auto Hasher = [&](size_t value) -> void { hash = Hash(hash, value); };

    Hasher(desc.magFilter);
    Hasher(desc.minFilter);
    Hasher(desc.mipmapMode);
    Hasher(desc.addressModeU);
    Hasher(desc.addressModeV);
    Hasher(desc.addressModeW);
    Hasher(std::hash<float>()(desc.mipLodBias));
    Hasher(static_cast<size_t>(desc.anisotropyEnable));
    Hasher(std::hash<float>()(desc.maxAnisotropy));
    Hasher(static_cast<size_t>(desc.compareEnable));
    Hasher(desc.compareOp);
    Hasher(std::hash<float>()(desc.minLod));
    Hasher(std::hash<float>()(desc.maxLod));
    Hasher(desc.borderColor);

    return hash;
  }
};

extern std::unordered_map<SamplerDescription, VkSampler, SamplerDescHash>
    SamplerCache; // NOLINT

auto GetOrCreateSampler(GraphicsContext &context,
                        const SamplerDescription &description) -> VkSampler;

auto DestroySamplers(GraphicsContext &context) -> void;

}; // namespace Graphics::Texture