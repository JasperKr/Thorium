#pragma once
#include "Graphics/format.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/error.hpp"
#include <cstring>
#include <public/tracy/Tracy.hpp>
#include <span>

#include "dds.hpp"
#include "stb/stb_image.h"
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cstdint>
#include <sys/types.h>
#include <vector>

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

static inline auto GetMipmapCount(VkExtent2D extent) -> uint32_t {
  uint32_t mipLevels = 1;
  while (extent.width > 1U || extent.height > 1U) {
    mipLevels++;
    extent.width = std::max(1U, extent.width >> 1U);
    extent.height = std::max(1U, extent.height >> 1U);
  }
  return mipLevels;
}

static inline auto GetMipmapCount(VkExtent3D extent) -> uint32_t {
  uint32_t mipLevels = 1;
  while (extent.width > 1U || extent.height > 1U || extent.depth > 1U) {
    mipLevels++;
    extent.width = std::max(1U, extent.width >> 1U);
    extent.height = std::max(1U, extent.height >> 1U);
    extent.depth = std::max(1U, extent.depth >> 1U);
  }
  return mipLevels;
}

static inline auto GetMipmapCount(uint32_t width, uint32_t height) -> uint32_t {
  return GetMipmapCount(VkExtent2D{width, height});
}

static inline auto GetMipmapCount(uint32_t width, uint32_t height,
                                  uint32_t depth) -> uint32_t {
  return GetMipmapCount(VkExtent3D{width, height, depth});
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

static inline auto GetDimensions(const VkExtent3D &extent, uint32_t mipLevel)
    -> VkExtent3D {
  return VkExtent3D{
      .width = std::max(1U, extent.width >> mipLevel),
      .height = std::max(1U, extent.height >> mipLevel),
      .depth = std::max(1U, extent.depth >> mipLevel),
  };
}

static inline auto GetDimensions(const VkExtent2D &extent, uint32_t mipLevel)
    -> VkExtent2D {
  return VkExtent2D{
      .width = std::max(1U, extent.width >> mipLevel),
      .height = std::max(1U, extent.height >> mipLevel),
  };
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

constexpr auto DDS_MAGIC = MakeFourCC('D', 'D', 'S', ' ');

inline auto IsDDS(const std::span<const uint8_t> &data) -> bool {

  if (data.size() < 4) {
    return false;
  }

  return (std::memcmp(data.data(), &DDS_MAGIC, 4) == 0);
}

inline auto ParseDDS(const std::span<const uint8_t> &data, int &outWidth,
                     int &outHeight, VkFormat &outFormat)
    -> Result<const std::span<const uint8_t>> {
  DDS_HEADER header{};
  memcpy(&header, data.data() + 4, sizeof(DDS_HEADER)); // NOLINT

  if (header.size != sizeof(DDS_HEADER)) {
    return Error::Unexpected("Invalid DDS header size.");
  }

  outWidth = static_cast<int>(header.width);
  outHeight = static_cast<int>(header.height);
  outFormat = DDSFourCCToVkFormat(header.ddspf.fourCC);

  const auto fourCCStr = std::string_view(
      reinterpret_cast<const char *>(&header.ddspf.fourCC), 4); // NOLINT
  if (outFormat != VK_FORMAT_UNDEFINED) {
    size_t headerSize = 4 + sizeof(DDS_HEADER);
    if (data.size() < headerSize) {
      return Error::Unexpected("DDS data is too small for headers.");
    }

    size_t pixelDataSize = data.size() - headerSize;
    // NOLINTNEXTLINE, safe because we check size above
    return std::span<const uint8_t>(data.data() + headerSize, pixelDataSize);
  }

  if (header.ddspf.fourCC != MakeFourCC('D', 'X', '1', '0')) {
    return Error::Unexpected("Unsupported DDS format (missing DX10 header).");
  }

  DDS_HEADER_DXT10 header10{};
  memcpy(&header10, data.data() + 4 + sizeof(DDS_HEADER), // NOLINT
         sizeof(DDS_HEADER_DXT10));

  auto formatResult = DXgiDDSFormatToVkFormat(header10.dxgiFormat);

  if (formatResult == VK_FORMAT_UNDEFINED) {
    return Error::Unexpected(
        "Unsupported DDS format (unsupported DXGI format).");
  }

  outFormat = formatResult;

  size_t headerSize = 4 + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
  if (data.size() < headerSize) {
    return Error::Unexpected("DDS data is too small for headers.");
  }

  size_t pixelDataSize = data.size() - headerSize;

  // NOLINTNEXTLINE, safe because we check size above
  return std::span<const uint8_t>(data.data() + headerSize, pixelDataSize);
}

// NOLINTNEXTLINE
inline auto FromMemory(const std::span<const uint8_t> &data, int &outWidth,
                       int &outHeight, VkFormat &outFormat, bool &requiresFree)
    -> Result<std::span<const uint8_t>> {
  ZoneScoped;

  int texChannels = 0;

  requiresFree = false;

  if (IsDDS(data)) {
    return ParseDDS(data, outWidth, outHeight, outFormat);
  }

  requiresFree = true;

  // check for LDR formats, supported by default stbi_load
  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) ==
      0) {
    ZoneScopedN("stbi_load_from_memory");

    stbi_uc *pixels = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()), &outWidth, &outHeight,
        &texChannels, STBI_rgb_alpha);

    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load image.");
    }

    outFormat = Graphics::DefaultPixelFormat;

    return std::span<uint8_t>(pixels, static_cast<size_t>(outWidth) *
                                          static_cast<size_t>(outHeight) *
                                          Graphics::Format::GetSize(outFormat));
  }

  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) !=
      0) {
    ZoneScopedN("stbi_loadf_from_memory");

    float *pixels =
        stbi_loadf_from_memory(data.data(), static_cast<int>(data.size()),
                               &outWidth, &outHeight, &texChannels, STBI_rgb);

    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load image.");
    }

    outFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;

    std::vector<uint8_t> convertedData(
        static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight) *
        Graphics::Format::GetSize(VK_FORMAT_B10G11R11_UFLOAT_PACK32));

    std::span<uint32_t> convertedPixels = std::span<uint32_t>(
        reinterpret_cast<uint32_t *>(convertedData.data()), // NOLINT
        static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight));

// Parallel for-loop to convert from RGB float to B10G11R11
#pragma omp parallel for
    for (int i = 0; i < outWidth * outHeight; i++) {
      int idx = i * 3;               // 3 channels (RGB)
      float red = pixels[idx];       // NOLINT
      float green = pixels[idx + 1]; // NOLINT
      float blue = pixels[idx + 2];  // NOLINT

      uint32_t lowp_red = Math::Float11::fromFloat(red).bits;
      uint32_t lowp_green = Math::Float11::fromFloat(green).bits;
      uint32_t lowp_blue = Math::Float10::fromFloat(blue).bits;

      convertedPixels[i] =
          (lowp_blue << 22U) | (lowp_green << 11U) | (lowp_red << 0U); // NOLINT
    }

    return std::span<uint8_t>(convertedData.data(), convertedData.size());
  }

  return Error::Unexpected("Unsupported image format.");
}

} // namespace Image