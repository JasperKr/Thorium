#pragma once
#include "Graphics/graphicsState.hpp"
#include "Modules/error.hpp"
#include <span>
#include <variant>
#define VK_NO_PROTOTYPES
#include "stb/stb_image.h"
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cstdint>

namespace Image {

// Returns total texel count for ALL mip levels, for ONE layer only.
// Caller must multiply by arrayLayers or 6 for cubemaps if needed.
static inline auto GetTexelCount(const VkExtent2D &extent,
                                 const uint32_t mipmapCount) -> uint64_t {
  uint64_t totalTexels = 0;
  for (uint64_t mip = 0; mip < mipmapCount; mip++) {
    uint64_t mipWidth = (std::max)(1U, extent.width >> mip);
    uint64_t mipHeight = (std::max)(1U, extent.height >> mip);

    totalTexels += mipWidth * mipHeight;
  }

  return totalTexels;
}

// Returns total texel count for ALL mip levels, for ONE layer only.
// Caller must multiply by arrayLayers or 6 for cubemaps if needed.
static inline auto GetTexelCount(const VkExtent3D &extent,
                                 const uint32_t mipmapCount) -> uint64_t {
  uint64_t totalTexels = 0;
  for (uint64_t mip = 0; mip < mipmapCount; mip++) {
    uint64_t mipWidth = (std::max)(1U, extent.width >> mip);
    uint64_t mipHeight = (std::max)(1U, extent.height >> mip);
    uint64_t mipDepth = (std::max)(1U, extent.depth >> mip);

    totalTexels += mipWidth * mipHeight * mipDepth;
  }

  return totalTexels;
}

static inline auto IsDepthTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

static inline auto IsStencilTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

static inline auto GetTextureAspectFlags(VkFormat format)
    -> VkImageAspectFlags {
  VkImageAspectFlags aspectFlags = 0;

  if (IsDepthTexture(format)) {
    aspectFlags |= static_cast<uint32_t>(VK_IMAGE_ASPECT_DEPTH_BIT);
  }

  if (IsStencilTexture(format)) {
    aspectFlags |= static_cast<uint32_t>(VK_IMAGE_ASPECT_STENCIL_BIT);
  }

  if (aspectFlags == 0) {
    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  return aspectFlags;
}

static inline auto IsCompressedTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
  case VK_FORMAT_BC2_UNORM_BLOCK:
  case VK_FORMAT_BC2_SRGB_BLOCK:
  case VK_FORMAT_BC3_UNORM_BLOCK:
  case VK_FORMAT_BC3_SRGB_BLOCK:
  case VK_FORMAT_BC4_UNORM_BLOCK:
  case VK_FORMAT_BC4_SNORM_BLOCK:
  case VK_FORMAT_BC5_UNORM_BLOCK:
  case VK_FORMAT_BC5_SNORM_BLOCK:
  case VK_FORMAT_BC6H_UFLOAT_BLOCK:
  case VK_FORMAT_BC6H_SFLOAT_BLOCK:
  case VK_FORMAT_BC7_UNORM_BLOCK:
  case VK_FORMAT_BC7_SRGB_BLOCK:
    return true;
  default:
    return false;
  }
}

using ImageLoadResultVariant = std::variant<float *, stbi_uc *>;

// NOLINTNEXTLINE
inline auto FromMemory(const std::span<const uint8_t> &data, int &outWidth,
                       int &outHeight, VkFormat &outFormat)
    -> Result<ImageLoadResultVariant> {
  int texWidth = 0;
  int texHeight = 0;
  int texChannels = 0;

  // check for LDR formats, supported by default stbi_load
  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) ==
      0) {
    stbi_uc *pixels = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()), &texWidth, &texHeight,
        &texChannels, STBI_rgb_alpha);

    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load image.");
    }

    outWidth = texWidth;
    outHeight = texHeight;
    outFormat = Graphics::DefaultPixelFormat;

    return pixels;
  }

  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) !=
      0) {
    float *pixels = stbi_loadf_from_memory(
        data.data(), static_cast<int>(data.size()), &texWidth, &texHeight,
        &texChannels, STBI_rgb_alpha); // force 4 channels

    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load image.");
    }

    outWidth = texWidth;
    outHeight = texHeight;
    outFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    return pixels;
  }

  return Error::Unexpected("Unsupported image format.");
}

} // namespace Image