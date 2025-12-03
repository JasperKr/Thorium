#pragma once

#include "Graphics/sampler.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphics.hpp"
#include "vulkan/vulkan_core.h"
#include <cmath>
#include <cstdint>

namespace Graphics::Texture {

enum class TextureType : uint8_t {
  DEFAULT, // 2D texture, but we cannot start a variable with a number
  VOLUME,
  CUBEMAP,
  ARRAY,
};

enum class WrapMode : uint8_t {
  REPEAT,
  MIRRORED_REPEAT,
  CLAMP,
  CLAMPONE,
  CLAMPZERO
};

const static Type type = Type("Texture");

struct Texture : Object {
  VkExtent3D size;
  VkFormat format;
  VkImage image;
  VkImageView view;
  VmaAllocation memory;
  uint64_t sizeInBytes;

  VkSampler sampler;
  SamplerDescription samplerDescription;
  bool samplerDirty;

  size_t mipmapcount;
  size_t arrayLayers;
  VkImageUsageFlags usage;

  enum TextureType textureType;

  auto SetFilter(VkFilter minFilter, VkFilter magFilter,
                 VkSamplerMipmapMode mipFilter) -> void;
  [[nodiscard]] auto GetFilter() const
      -> std::tuple<VkFilter, VkFilter, VkSamplerMipmapMode>;
  auto SetAnisotropy(float anisotropy) -> void;
  [[nodiscard]] auto GetAnisotropy() const -> float;
  auto SetWrapmode(VkSamplerAddressMode addressModeU,
                   VkSamplerAddressMode addressModeV,
                   VkSamplerAddressMode addressModeW) -> void;
  [[nodiscard]] auto GetWrapmode() const
      -> std::tuple<VkSamplerAddressMode, VkSamplerAddressMode,
                    VkSamplerAddressMode>;
  auto SetLodBias(float mipLodBias) -> void;
  [[nodiscard]] auto GetLodBias() const -> float;
  auto SetLodRange(float minLod, float maxLod) -> void;
  [[nodiscard]] auto GetLodRange() const -> std::tuple<float, float>;
  auto SetDepthCompare(bool enable, VkCompareOp compareOp) -> void;
  [[nodiscard]] auto GetDepthCompare() const -> std::tuple<bool, VkCompareOp>;
  [[nodiscard]] auto GetWidth() const -> uint32_t { return size.width; };
  [[nodiscard]] auto GetHeight() const -> uint32_t { return size.height; };
  [[nodiscard]] auto GetDimensions() const -> VkExtent2D {
    return {size.width, size.height};
  };
  [[nodiscard]] auto GetDepth() const -> uint32_t { return size.depth; };
  auto GetSampler(GraphicsContext &context) -> VkSampler;
  auto SetPixels(GraphicsContext &context, Image::ImageData &imageData,
                 uint32_t mipLevel, uint32_t arrayLayer, VkRect2D source,
                 VkOffset2D target) -> Error::Error;
  auto SetPixels(GraphicsContext &context, Image::ImageData &imageData,
                 uint32_t mipLevel, uint32_t arrayLayer) -> Error::Error;

  static auto GetType() -> Type const * { return &type; }
};

struct TextureCreationInfo {
  uint32_t width = 0;  // Width in pixels
  uint32_t height = 0; // Height in pixels
  uint32_t depth{};    // Depth in pixels (for 3D textures, or Array layers)
  VkFormat format = VK_FORMAT_UNDEFINED; // Texture format
  VkImageUsageFlags usage{};             // Vulkan usage flags
  int mipmapCount{};                     // Number of mipmap levels
};

auto Create2D(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Ref<Texture>, Error::Error>;
auto FromSwapchainTexture(GraphicsContext &context, VkImage swapchainImage,
                          VkFormat format, uint32_t width, uint32_t height)
    -> tl::expected<Ref<Texture>, Error::Error>;
auto CreateCubeMap(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Ref<Texture>, Error::Error>;
auto CreateVolume(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Ref<Texture>, Error::Error>;
auto CreateArray(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Ref<Texture>, Error::Error>;

void Destroy(GraphicsContext &context, Texture *texture);
auto TransitionLayout(GraphicsContext &context, Texture *texture,
                      VkImageLayout oldLayout, VkImageLayout newLayout)
    -> Error::Error;
auto CopyBufferToImage(GraphicsContext &context, VkBuffer buffer,
                       Texture *texture, VkBufferImageCopy region)
    -> Error::Error;
auto CopyImageToBuffer(GraphicsContext &context, Texture *texture,
                       VkBuffer buffer) -> Error::Error;
auto GenerateMipmaps(GraphicsContext &context, Texture *texture)
    -> Error::Error;
auto LoadFromFile(GraphicsContext &context, const char *path)
    -> tl::expected<Ref<Texture>, Error::Error>;

// texture 2D From byte array
auto LoadFromMemory(GraphicsContext &context, const unsigned char *data,
                    size_t dataSize, VkFormat format)
    -> tl::expected<Ref<Texture>, Error::Error>;

// texture 2D From ImageData
auto LoadFromMemory(GraphicsContext &context, Image::ImageData &imageData)
    -> tl::expected<Ref<Texture>, Error::Error>;

// texture 3D/Array/Cubemap From array of ImageData slices
auto LoadFromMemory(GraphicsContext &context,
                    const std::vector<Image::ImageData *> &slices,
                    TextureType type)
    -> tl::expected<Ref<Texture>, Error::Error>;

static inline auto GetMipChainCostMultiplier(uint32_t mipLevels,
                                             bool volume = false) -> double {
  if (mipLevels <= 1) {
    return 1.0F;
  }

  const double costReductionPerMip = volume ? 0.125F : 0.25F;
  // Sum of geometric series: 1 + 1/4 + 1/16 + ... + 1/(4^(mipLevels-1))

  return (1.0F -
          std::pow(costReductionPerMip, static_cast<double>(mipLevels))) /
         (1.0F - costReductionPerMip);
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

auto GetDefaultTexture(GraphicsContext &context, VkFormat format,
                       Graphics::Texture::TextureType textureType)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error>;

} // namespace Graphics::Texture
