#pragma once
#include "vulkan/vulkan_core.h"
#include <span>

namespace Image {

// Returns total texel count for ALL mip levels, for ONE layer only.
// Caller must multiply by arrayLayers or 6 for cubemaps if needed.
auto GetTexelCount(const VkExtent2D &extent, uint32_t mipmapCount) -> uint64_t;

// Returns total texel count for ALL mip levels, for ONE layer only.
// Caller must multiply by arrayLayers or 6 for cubemaps if needed.
auto GetTexelCount(const VkExtent3D &extent, uint32_t mipmapCount) -> uint64_t;

auto GetMipmapCount(VkExtent2D extent) -> uint32_t;

auto GetMipmapCount(VkExtent3D extent) -> uint32_t;

auto GetMipmapCount(uint32_t width, uint32_t height) -> uint32_t;

auto GetMipmapCount(uint32_t width, uint32_t height, uint32_t depth)
    -> uint32_t;

auto IsDepthTexture(VkFormat format) -> bool;

auto IsStencilTexture(VkFormat format) -> bool;

auto GetTextureAspectFlags(VkFormat format) -> VkImageAspectFlags;

auto GetDimensions(const VkExtent3D &extent, uint32_t mipLevel) -> VkExtent3D;

auto GetDimensions(const VkExtent2D &extent, uint32_t mipLevel) -> VkExtent2D;

auto IsCompressedTexture(VkFormat format) -> bool;

auto IsDDS(const std::span<const uint8_t> &data) -> bool;

} // namespace Image