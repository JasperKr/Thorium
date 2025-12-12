#pragma once

#include "Graphics/sampler.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphics.hpp"
#include "resource.hpp"
#include "vulkan/vulkan_core.h"
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

  bool released;
  uint64_t lastUsedTimelineValue;

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
  [[nodiscard]] auto GetWrap() const
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
  [[nodiscard]] auto GetMipmapCount() const -> size_t { return mipmapcount; }
  [[nodiscard]] auto GetFormat() const -> VkFormat { return format; }

  static auto GetType() -> Type const * { return &type; }

  // Release the resources for safe automatic destruction later
  auto Release() -> bool;

  // Destroy the texture immediately, use with caution
  auto Destroy(GraphicsContext &context) const -> void;
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
auto LoadFromFile(GraphicsContext &context, const char *path,
                  VkImageUsageFlags usage = 0)
    -> tl::expected<Ref<Texture>, Error::Error>;

// texture 2D From byte array
auto LoadFromMemory(GraphicsContext &context, const unsigned char *data,
                    size_t dataSize, VkFormat format,
                    VkImageUsageFlags usage = 0)
    -> tl::expected<Ref<Texture>, Error::Error>;

// texture 2D From ImageData
auto LoadFromMemory(GraphicsContext &context, Image::ImageData &imageData,
                    VkImageUsageFlags usage = 0)
    -> tl::expected<Ref<Texture>, Error::Error>;

// texture 3D/Array/Cubemap From array of ImageData slices
auto LoadFromMemory(GraphicsContext &context,
                    const std::vector<Image::ImageData *> &slices,
                    TextureType type, VkImageUsageFlags usage = 0)
    -> tl::expected<Ref<Texture>, Error::Error>;

auto GetDefaultTexture(GraphicsContext &context, VkFormat format,
                       Graphics::Texture::TextureType textureType)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error>;

} // namespace Graphics::Texture
