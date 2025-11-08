#pragma once

#include "graphics.hpp"
namespace Graphics {

enum TextureType : uint8_t {
  TEXTURE_TYPE_2D,
  TEXTURE_TYPE_VOLUME,
  TEXTURE_TYPE_CUBE_MAP,
  TEXTURE_TYPE_ARRAY,
};

struct Texture {
  VkExtent3D size;
  VkFormat format;
  VkImage image;
  VkImageView view;
  VmaAllocation memory;

  enum TextureType type;
};

struct TextureCreationInfo {
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  VkFormat format;
  VkImageUsageFlags usage;
  int mipmapCount;
};

auto Texture_Create2D(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error>;
auto Texture_CreateCubeMap(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error>;
auto Texture_CreateVolume(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error>;
auto Texture_CreateArray(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error>;

void Texture_Destroy(GraphicsContext &context, Texture *texture);
auto Texture_TransitionLayout(GraphicsContext &context, Texture *texture,
                              VkImageLayout oldLayout, VkImageLayout newLayout)
    -> Error::Error;
auto Texture_CopyBufferToImage(GraphicsContext &context, VkBuffer buffer,
                               Texture *texture) -> Error::Error;
auto Texture_CopyImageToBuffer(GraphicsContext &context, Texture *texture,
                               VkBuffer buffer) -> Error::Error;
auto Texture_GenerateMipmaps(GraphicsContext &context, Texture *texture)
    -> Error::Error;
auto Texture_LoadFromFile(GraphicsContext &context, const char *path,
                          Texture *outTexture) -> Error::Error;
auto Texture_LoadFromMemory(GraphicsContext &context, const unsigned char *data,
                            size_t dataSize, Texture *outTexture)
    -> Error::Error;
} // namespace Graphics