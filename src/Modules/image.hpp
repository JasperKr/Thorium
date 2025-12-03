#pragma once

#include "vulkan/vulkan_core.h"
#include <cstdint>

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
} // namespace Image