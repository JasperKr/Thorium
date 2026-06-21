#include "Modules/dds.hpp"
#include <algorithm>
#include <cstring>
#include <span>
#include <vulkan/vulkan_core.h>

namespace Image {

auto IsDepthTexture(VkFormat format) -> bool {
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

auto IsStencilTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

auto IsDepthOrStencilTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
  case VK_FORMAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

auto GetTextureAspectFlags(VkFormat format) -> VkImageAspectFlags {
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

auto GetDimensions(const VkExtent3D &extent, uint32_t mipLevel) -> VkExtent3D {
  return VkExtent3D{
      .width = std::max(1U, extent.width >> mipLevel),
      .height = std::max(1U, extent.height >> mipLevel),
      .depth = std::max(1U, extent.depth >> mipLevel),
  };
}

auto GetDimensions(const VkExtent2D &extent, uint32_t mipLevel) -> VkExtent2D {
  return VkExtent2D{
      .width = std::max(1U, extent.width >> mipLevel),
      .height = std::max(1U, extent.height >> mipLevel),
  };
}

auto IsCompressedTexture(VkFormat format) -> bool {
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

constexpr auto DDS_MAGIC = MakeFourCC('D', 'D', 'S', ' ');

auto IsDDS(const std::span<const uint8_t> &data) -> bool {
  if (data.size() < 4) {
    return false;
  }

  return (std::memcmp(data.data(), &DDS_MAGIC, 4) == 0);
}

} // namespace Image