#pragma once

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <string>

namespace Image {

static inline auto GetFormatChannelCount(VkFormat format) -> uint32_t {
  switch (format) {
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SRGB:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32_SFLOAT:
    return 1;

  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SRGB:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32_SFLOAT:
    return 2;

  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_B8G8R8_UNORM:
  case VK_FORMAT_B8G8R8_SRGB:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
    return 3;

  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_SRGB:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16A16_SFLOAT:
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return 4;
  default:
    return 0;
  }
}

static inline auto GetFormatSize(VkFormat format) -> uint32_t {
  constexpr uint32_t byteSize = 1;
  constexpr uint32_t shortSize = 2;
  constexpr uint32_t halfSize = 2; // FP16
  constexpr uint32_t floatSize = 4;
  constexpr uint32_t intSize = 4;

  constexpr uint32_t BC1BlockSize = 8;
  constexpr uint32_t BC2BlockSize = 16;
  constexpr uint32_t BC3BlockSize = 16;
  constexpr uint32_t BC4BlockSize = 8;
  constexpr uint32_t BC5BlockSize = 16;
  constexpr uint32_t BC6HBlockSize = 16;
  constexpr uint32_t BC7BlockSize = 16;

  switch (format) {
  //
  // --- 1 CHANNEL ---
  //
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SRGB:
    return byteSize * 1;

  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_SINT:
    return shortSize * 1;

  case VK_FORMAT_R16_SFLOAT:
    return halfSize * 1;

  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32_SFLOAT:
    return floatSize * 1;

  //
  // --- 2 CHANNEL ---
  //
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SRGB:
    return byteSize * 2;

  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_SINT:
    return shortSize * 2;

  case VK_FORMAT_R16G16_SFLOAT:
    return halfSize * 2;

  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32_SFLOAT:
    return floatSize * 2;

  //
  // --- 3 CHANNEL ---
  //
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_B8G8R8_UNORM:
  case VK_FORMAT_B8G8R8_SRGB:
    return byteSize * 3;

  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SFLOAT:
    return shortSize * 3;

  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
    return floatSize * 3;

  //
  // --- 4 CHANNEL ---
  //
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_SRGB:
    return byteSize * 4;

  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SINT:
    return shortSize * 4;

  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return halfSize * 4;

  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return floatSize * 4;

  //
  // --- Depth / Stencil ---
  //
  case VK_FORMAT_D16_UNORM:
    return shortSize;
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return (3 * byteSize) + byteSize;
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return intSize;
  case VK_FORMAT_D32_SFLOAT:
    return floatSize;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return floatSize + byteSize;

  //
  // --- BC Compressed (block size) ---
  //
  case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    return BC1BlockSize;

  case VK_FORMAT_BC2_UNORM_BLOCK:
  case VK_FORMAT_BC2_SRGB_BLOCK:
    return BC2BlockSize;
  case VK_FORMAT_BC3_UNORM_BLOCK:
  case VK_FORMAT_BC3_SRGB_BLOCK:
    return BC3BlockSize;

  case VK_FORMAT_BC4_UNORM_BLOCK:
  case VK_FORMAT_BC4_SNORM_BLOCK:
    return BC4BlockSize;

  case VK_FORMAT_BC5_UNORM_BLOCK:
  case VK_FORMAT_BC5_SNORM_BLOCK:
    return BC5BlockSize;

  case VK_FORMAT_BC6H_UFLOAT_BLOCK:
  case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    return BC6HBlockSize;
  case VK_FORMAT_BC7_UNORM_BLOCK:
  case VK_FORMAT_BC7_SRGB_BLOCK:
    return BC7BlockSize;

  default:
    return 0;
  }
}

// NOLINTNEXTLINE string -> VkFormat
static inline auto StringToFormat(const std::string &format) -> VkFormat {
  // 8-bit unorm
  if (format == "rgba8") {
    return VK_FORMAT_R8G8B8A8_UNORM;
  }
  if (format == "rg8") {
    return VK_FORMAT_R8G8_UNORM;
  }
  if (format == "r8") {
    return VK_FORMAT_R8_UNORM;
  }
  // 16-bit unorm
  if (format == "rgba16") {
    return VK_FORMAT_R16G16B16A16_UNORM;
  }
  if (format == "rg16") {
    return VK_FORMAT_R16G16_UNORM;
  }
  if (format == "r16") {
    return VK_FORMAT_R16_UNORM;
  }
  // 16-bit float
  if (format == "rgba16f") {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  if (format == "rg16f") {
    return VK_FORMAT_R16G16_SFLOAT;
  }
  if (format == "r16f") {
    return VK_FORMAT_R16_SFLOAT;
  }
  // 32-bit float
  if (format == "rgba32f") {
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  if (format == "rg32f") {
    return VK_FORMAT_R32G32_SFLOAT;
  }
  if (format == "r32f") {
    return VK_FORMAT_R32_SFLOAT;
  }
  // 8-bit uint
  if (format == "rgba8ui") {
    return VK_FORMAT_R8G8B8A8_UINT;
  }
  if (format == "rg8ui") {
    return VK_FORMAT_R8G8_UINT;
  }
  if (format == "r8ui") {
    return VK_FORMAT_R8_UINT;
  }
  // 16-bit uint
  if (format == "rgba16ui") {
    return VK_FORMAT_R16G16B16A16_UINT;
  }
  if (format == "rg16ui") {
    return VK_FORMAT_R16G16_UINT;
  }
  if (format == "r16ui") {
    return VK_FORMAT_R16_UINT;
  }
  // 32-bit uint
  if (format == "rgba32ui") {
    return VK_FORMAT_R32G32B32A32_UINT;
  }
  if (format == "rg32ui") {
    return VK_FORMAT_R32G32_UINT;
  }
  if (format == "r32ui") {
    return VK_FORMAT_R32_UINT;
  }
  // 8-bit sint
  if (format == "rgba8si") {
    return VK_FORMAT_R8G8B8A8_SINT;
  }
  if (format == "rg8si") {
    return VK_FORMAT_R8G8_SINT;
  }
  if (format == "r8si") {
    return VK_FORMAT_R8_SINT;
  }
  // 16-bit sint
  if (format == "rgba16si") {
    return VK_FORMAT_R16G16B16A16_SINT;
  }
  if (format == "rg16si") {
    return VK_FORMAT_R16G16_SINT;
  }
  if (format == "r16si") {
    return VK_FORMAT_R16_SINT;
  }
  // 32-bit sint
  if (format == "rgba32si") {
    return VK_FORMAT_R32G32B32A32_SINT;
  }
  if (format == "rg32si") {
    return VK_FORMAT_R32G32_SINT;
  }
  if (format == "r32si") {
    return VK_FORMAT_R32_SINT;
  }
  // Depth formats
  if (format == "depth16") {
    return VK_FORMAT_D16_UNORM;
  }
  if (format == "depth24") {
    return VK_FORMAT_X8_D24_UNORM_PACK32;
  }
  if (format == "depth32") {
    return VK_FORMAT_D32_SFLOAT;
  }
  // Depth-stencil formats
  if (format == "depth24stencil8") {
    return VK_FORMAT_D24_UNORM_S8_UINT;
  }
  if (format == "depth32stencil8") {
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  }
  // packed formats
  if (format == "rg11b10f") {
    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
  }
  if (format == "rgb9e5") {
    return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
  }
  if (format == "rgb10a2") {
    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
  }
  if (format == "rgb10a2ui") {
    return VK_FORMAT_A2B10G10R10_UINT_PACK32;
  }
  if (format == "bgr5a1") {
    return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
  }
  if (format == "bgr565") {
    return VK_FORMAT_R5G6B5_UNORM_PACK16;
  }
  if (format == "rgba4") {
    return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
  }
  // compressed formats
  if (format == "bc1") {
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  }
  if (format == "bc3") {
    return VK_FORMAT_BC3_UNORM_BLOCK;
  }
  if (format == "bc4") {
    return VK_FORMAT_BC4_UNORM_BLOCK;
  }
  if (format == "bc5") {
    return VK_FORMAT_BC5_UNORM_BLOCK;
  }
  if (format == "bc6h") {
    return VK_FORMAT_BC6H_UFLOAT_BLOCK;
  }
  if (format == "bc6hs") {
    return VK_FORMAT_BC6H_SFLOAT_BLOCK;
  }
  if (format == "bc7") {
    return VK_FORMAT_BC7_UNORM_BLOCK;
  }

  return VK_FORMAT_UNDEFINED;
}

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

} // namespace Image