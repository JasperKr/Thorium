#include "texture.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/resource.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/image.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "sampler.hpp"
#include "stb/stb_image.h"
#include "tl/expected.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#define VMA_VULKAN_VERSION 1004000
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <vma/vk_mem_alloc.h>

namespace Graphics::Texture {

auto GetAspectFlagsForFormat(VkFormat format) -> VkImageAspectFlagBits {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D32_SFLOAT:
    return VK_IMAGE_ASPECT_DEPTH_BIT;

  case VK_FORMAT_S8_UINT:
    return VK_IMAGE_ASPECT_STENCIL_BIT;

  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return static_cast<VkImageAspectFlagBits>(
        static_cast<uint32_t>(VK_IMAGE_ASPECT_DEPTH_BIT) |
        static_cast<uint32_t>(VK_IMAGE_ASPECT_STENCIL_BIT));

  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

inline auto SetDebugName(const std::string &debugName, Texture *texture,
                         const GraphicsContext &context) -> Error {

  auto debugname = Graphics::ContextDebugname + "_" + debugName;

  VkDebugUtilsObjectNameInfoEXT nameInfo = {};
  nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
  nameInfo.objectHandle = static_cast<uint64_t>(
      reinterpret_cast<uintptr_t>(texture->image)), // NOLINT
      nameInfo.pObjectName = debugname.c_str();
  auto error =
      Error::Create(vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo));
  if (Error::IsError(error)) {
    return error;
  }

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    vmaSetAllocationName(context.vmaAllocator, texture->memory,
                         debugname.c_str());
  }

  return Error::Success();
}

auto Create2D(const GraphicsContext &context, const TextureCreationInfo &info)
    -> Result<Ref<Texture>> {

  Ref<Texture> texture = Ref<Texture>::Make();

  texture->size = VkExtent3D{info.width, info.height, 1};
  texture->format = info.format;
  texture->textureType = TextureType::DEFAULT;
  texture->mipmapcount = info.mipmapCount;
  texture->usage = info.usage;
  texture->arrayLayers = 1;
  texture->samplerDirty = true;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = info.width;
  imageInfo.extent.height = info.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = info.mipmapCount;
  imageInfo.arrayLayers = 1;
  imageInfo.format = info.format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = info.usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.requiredFlags = 0;
  allocInfo.preferredFlags = 0;

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    Error error = Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo,
                                               &allocInfo, &texture->image,
                                               &texture->memory, nullptr));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  auto setNameResult = SetDebugName(info.debugName, texture.get(), context);
  if (Error::IsError(setNameResult)) {
    return setNameResult.AsUnexpected();
  }

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;
  viewInfo.components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .a = VK_COMPONENT_SWIZZLE_IDENTITY};

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto error = Error::Create(
        vkCreateImageView(context.device, &viewInfo, nullptr, &texture->view));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  VmaAllocationInfo memRequirements;
  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    vmaGetAllocationInfo(context.vmaAllocator, texture->memory,
                         &memRequirements);
  }
  texture->sizeInBytes = memRequirements.size;

  return texture;
}

auto FromSwapchainTexture(const GraphicsContext &context,
                          VkImage swapchainImage,
                          VkImageView swapchainImageView, VkFormat format,
                          uint32_t width, uint32_t height)
    -> Result<Ref<Texture>> {

  Ref<Texture> texture = Ref<Texture>::Make();

  texture->image = swapchainImage;
  texture->format = format;
  texture->size = VkExtent3D{width, height, 1};
  texture->textureType = TextureType::DEFAULT;
  texture->mipmapcount = 1;
  texture->arrayLayers = 1;
  texture->samplerDirty = true;
  texture->isSwapchainView = true;

  VkSurfaceCapabilitiesKHR surfaceCapabilities;

  auto error = Error::Create(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      context.physicalDevice, context.surface, &surfaceCapabilities));

  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  texture->usage = surfaceCapabilities.supportedUsageFlags;
  texture->view = swapchainImageView;

  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  return texture;
}

auto CreateCubeMap(const GraphicsContext &context,
                   const TextureCreationInfo &info) -> Result<Ref<Texture>> {

  if (info.width != info.height) {
    return Error::Unexpected(
        "Cube map textures must have equal width and height.");
  }
  const int CubeFaceCount = 6;

  Ref<Texture> texture = Ref<Texture>::Make();

  texture->size = VkExtent3D{info.width, info.width, 1};
  texture->format = info.format;
  texture->textureType = TextureType::CUBEMAP;
  texture->mipmapcount = info.mipmapCount;
  texture->usage = info.usage;
  texture->arrayLayers = CubeFaceCount;
  texture->samplerDirty = true;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = info.width;
  imageInfo.extent.height = info.width;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = info.mipmapCount;
  imageInfo.arrayLayers = CubeFaceCount;
  imageInfo.format = info.format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = info.usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.requiredFlags = 0;
  allocInfo.preferredFlags = 0;

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    Error error = Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo,
                                               &allocInfo, &texture->image,
                                               &texture->memory, nullptr));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  auto setNameResult = SetDebugName(info.debugName, texture.get(), context);
  if (Error::IsError(setNameResult)) {
    return setNameResult.AsUnexpected();
  }

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = CubeFaceCount;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto error = Error::Create(
        vkCreateImageView(context.device, &viewInfo, nullptr, &texture->view));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  VmaAllocationInfo memRequirements;
  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    vmaGetAllocationInfo(context.vmaAllocator, texture->memory,
                         &memRequirements);
  }
  texture->sizeInBytes = memRequirements.size;

  return texture;
}

auto CreateVolume(const GraphicsContext &context,
                  const TextureCreationInfo &info) -> Result<Ref<Texture>> {

  Ref<Texture> texture = Ref<Texture>::Make();

  texture->size = VkExtent3D{info.width, info.height, info.depth};
  texture->format = info.format;
  texture->textureType = TextureType::VOLUME;
  texture->mipmapcount = info.mipmapCount;
  texture->usage = info.usage;
  texture->arrayLayers = 1;
  texture->samplerDirty = true;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_3D;
  imageInfo.extent.width = info.width;
  imageInfo.extent.height = info.height;
  imageInfo.extent.depth = info.depth;
  imageInfo.mipLevels = info.mipmapCount;
  imageInfo.arrayLayers = 1;
  imageInfo.format = info.format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = info.usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.requiredFlags = 0;
  allocInfo.preferredFlags = 0;

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    Error error = Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo,
                                               &allocInfo, &texture->image,
                                               &texture->memory, nullptr));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  auto setNameResult = SetDebugName(info.debugName, texture.get(), context);
  if (Error::IsError(setNameResult)) {
    return setNameResult.AsUnexpected();
  }

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto error = Error::Create(
        vkCreateImageView(context.device, &viewInfo, nullptr, &texture->view));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  VmaAllocationInfo memRequirements;
  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    vmaGetAllocationInfo(context.vmaAllocator, texture->memory,
                         &memRequirements);
  }
  texture->sizeInBytes = memRequirements.size;

  return texture;
}

auto CreateArray(const GraphicsContext &context,
                 const TextureCreationInfo &info) -> Result<Ref<Texture>> {

  Ref<Texture> texture = Ref<Texture>::Make();

  texture->size = VkExtent3D{info.width, info.height, info.depth};
  texture->format = info.format;
  texture->textureType = TextureType::ARRAY;
  texture->mipmapcount = info.mipmapCount;
  texture->usage = info.usage;
  texture->arrayLayers = info.depth;
  texture->samplerDirty = true;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = info.width;
  imageInfo.extent.height = info.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = info.mipmapCount;
  imageInfo.arrayLayers = info.depth;
  imageInfo.format = info.format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = info.usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.requiredFlags = 0;
  allocInfo.preferredFlags = 0;

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    Error error = Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo,
                                               &allocInfo, &texture->image,
                                               &texture->memory, nullptr));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  auto setNameResult = SetDebugName(info.debugName, texture.get(), context);
  if (Error::IsError(setNameResult)) {
    return setNameResult.AsUnexpected();
  }

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = info.depth;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto error = Error::Create(
        vkCreateImageView(context.device, &viewInfo, nullptr, &texture->view));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  VmaAllocationInfo memRequirements;
  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    vmaGetAllocationInfo(context.vmaAllocator, texture->memory,
                         &memRequirements);
  }
  texture->sizeInBytes = memRequirements.size;

  return texture;
}

auto LoadFromFile(GraphicsContext &context, const char *path,
                  VkImageUsageFlags usage) -> Result<Ref<Texture>> {
  auto fileLoadResult = Filesystem::ReadFile(path);

  if (Error::IsError(fileLoadResult)) {
    return fileLoadResult.error().AsUnexpected();
  }

  auto filedata = fileLoadResult.value();
  auto dataSpan =
      std::span<uint8_t>(filedata.data(), static_cast<size_t>(filedata.size()));

  auto imageDataResult = Image::ImageData::Create(dataSpan);

  if (Error::IsError(imageDataResult)) {
    return imageDataResult.error().AsUnexpected();
  }

  auto imageData = imageDataResult.value();

  auto texture = Create2D(
      context, TextureCreationInfo{
                   .width = imageData->GetWidth(),
                   .height = imageData->GetHeight(),
                   .format = imageData->GetFormat(),
                   .usage = usage | static_cast<uint32_t>(
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                   .mipmapCount = 1,
                   .debugName = "Image_" + std::string(path),
               });

  if (Error::IsError(texture)) {
    return texture.error().AsUnexpected();
  }

  auto result = texture.value()->SetPixels(context, *imageData);

  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  return texture;
}

// texture 2D From byte array
auto LoadFromMemory(GraphicsContext &context,
                    const std::span<const uint8_t> &data,
                    VkImageUsageFlags usage) -> Result<Ref<Texture>> {

  auto width = 0;
  auto height = 0;
  auto format = VK_FORMAT_UNDEFINED;

  auto loadResult = Image::FromMemory(data, width, height, format);
  if (Error::IsError(loadResult)) {
    return loadResult.error().AsUnexpected();
  }

  auto texture = Create2D(
      context, TextureCreationInfo{
                   .width = static_cast<uint32_t>(width),
                   .height = static_cast<uint32_t>(height),
                   .format = format,
                   .usage = usage | static_cast<uint32_t>(
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                   .mipmapCount = 1,
               });

  if (Error::IsError(texture)) {
    return texture.error().AsUnexpected();
  }

  std::span<uint8_t> dataSpan;

  auto variant = loadResult.value();
  if (std::holds_alternative<stbi_uc *>(variant)) {
    auto *pixels = std::get<stbi_uc *>(variant);
    dataSpan = std::span<uint8_t>(pixels, static_cast<size_t>(width) *
                                              static_cast<size_t>(height) *
                                              Format::GetSize(format));

  } else if (std::holds_alternative<float *>(variant)) {
    auto *pixels = std::get<float *>(variant);

    // NOLINTNEXTLINE, reinterpret cast is safe here
    dataSpan = std::span<uint8_t>(reinterpret_cast<uint8_t *>(pixels),
                                  static_cast<size_t>(width) *
                                      static_cast<size_t>(height) *
                                      Format::GetSize(format));

  } else {
    return Error::Unexpected("Unsupported image data format.");
  }

  auto source = VkRect2D{
      .offset = VkOffset2D{0, 0},
      .extent = VkExtent2D{static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height)},
  };

  auto dst = VkOffset2D{0, 0};

  auto result = texture.value()->SetPixels(context, dataSpan, width, height, 0,
                                           0, source, dst);

  if (std::holds_alternative<stbi_uc *>(variant)) {
    auto *pixels = std::get<stbi_uc *>(variant);
    stbi_image_free(pixels);
  } else if (std::holds_alternative<float *>(variant)) {
    auto *pixels = std::get<float *>(variant);
    stbi_image_free(pixels);
  }

  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  return texture;
}

// texture 2D From ImageData
auto LoadFromMemory(GraphicsContext &context, Image::ImageData &imageData,
                    VkImageUsageFlags usage) -> Result<Ref<Texture>> {
  auto texture = Create2D(
      context, TextureCreationInfo{
                   .width = imageData.GetWidth(),
                   .height = imageData.GetHeight(),
                   .format = imageData.GetFormat(),
                   .usage = usage | static_cast<uint32_t>(
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                   .mipmapCount = 1,
               });

  if (Error::IsError(texture)) {
    return texture.error().AsUnexpected();
  }

  auto result = texture.value()->SetPixels(context, imageData, 0, 0);
  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  return texture;
}

// texture 3D/Array/Cubemap From array of ImageData slices
auto LoadFromMemory(GraphicsContext &context,
                    const std::vector<Image::ImageData *> &slices,
                    TextureType type, VkImageUsageFlags usage)
    -> Result<Ref<Texture>> {
  if (slices.empty()) {
    return Error::Unexpected("No image slices provided.");
  }

  uint32_t width = slices[0]->GetWidth();
  uint32_t height = slices[0]->GetHeight();

  Ref<Texture> texture;

  switch (type) {
  case TextureType::CUBEMAP: {
    if (slices.size() != 6) { // NOLINT
      return Error::Unexpected("Cubemap textures require 6 image slices.");
    }

    auto cubeMapTexture = CreateCubeMap(
        context, TextureCreationInfo{
                     .width = width,
                     .height = height,
                     .format = slices[0]->GetFormat(),
                     .usage = usage | static_cast<uint32_t>(
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                     .mipmapCount = 1,
                 });

    if (Error::IsError(cubeMapTexture)) {
      return cubeMapTexture.error().AsUnexpected();
    }

    texture = cubeMapTexture.value();
    break;
  }
  case TextureType::ARRAY: {
    auto arrayTexture = CreateArray(
        context, TextureCreationInfo{
                     .width = width,
                     .height = height,
                     .depth = static_cast<uint32_t>(slices.size()),
                     .format = slices[0]->GetFormat(),
                     .usage = usage | static_cast<uint32_t>(
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                     .mipmapCount = 1,
                 });

    if (Error::IsError(arrayTexture)) {
      return arrayTexture.error().AsUnexpected();
    }

    texture = arrayTexture.value();
    break;
  }
  case TextureType::VOLUME: {
    auto volumeTexture = CreateVolume(
        context, TextureCreationInfo{
                     .width = width,
                     .height = height,
                     .depth = static_cast<uint32_t>(slices.size()),
                     .format = slices[0]->GetFormat(),
                     .usage = usage | static_cast<uint32_t>(
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                     .mipmapCount = 1,
                 });

    if (Error::IsError(volumeTexture)) {
      return volumeTexture.error().AsUnexpected();
    }

    texture = volumeTexture.value();
    break;
  }
  default:
    return Error::Unexpected(
        "Unsupported texture type for multiple image slices.");
  }

  for (size_t i = 0; i < slices.size(); ++i) {
    auto result =
        texture->SetPixels(context, *slices[i], 0, static_cast<uint32_t>(i),
                           VkRect2D{
                               .offset = VkOffset2D{0, 0},
                               .extent = VkExtent2D{width, height},
                           },
                           VkOffset2D{0, 0});
    if (Error::IsError(result)) {
      return result.AsUnexpected();
    }
  }

  return texture;
}

auto ImageLayoutToString(VkImageLayout layout) -> const char * {
  switch (layout) {
  case VK_IMAGE_LAYOUT_UNDEFINED:
    return "UNDEFINED";
  case VK_IMAGE_LAYOUT_GENERAL:
    return "GENERAL";
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    return "COLOR_ATTACHMENT_OPTIMAL";
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    return "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    return "SHADER_READ_ONLY_OPTIMAL";
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    return "TRANSFER_SRC_OPTIMAL";
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    return "TRANSFER_DST_OPTIMAL";
  default:
    return "UNKNOWN_LAYOUT";
  }
}

auto GetAccessMask(VkImageLayout layout) -> VkAccessFlags {
  switch (layout) {
  case VK_IMAGE_LAYOUT_GENERAL:
    return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    return VK_ACCESS_SHADER_READ_BIT;
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    return VK_ACCESS_TRANSFER_READ_BIT;
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    return VK_ACCESS_TRANSFER_WRITE_BIT;
  case VK_IMAGE_LAYOUT_UNDEFINED:
    return 0;
  default:
    PrintWarning("GetAccessMask: Unsupported layout {}",
                 ImageLayoutToString(layout));
    return 0;
  }
}

auto Texture::TransitionLayout(GraphicsContext &context, VkImageLayout layout,
                               VkPipelineStageFlags2 sourceStage, // NOLINT
                               VkPipelineStageFlags2 destinationStage,
                               VkAccessFlags2 srcAccessMask, // NOLINT
                               VkAccessFlags2 dstAccessMask) -> Error {

  if (layout == VK_IMAGE_LAYOUT_UNDEFINED) {
    return Error::Create("Cannot transition to UNDEFINED layout.");
  }

  if (currentLayout == layout && sourceStage == destinationStage &&
      srcAccessMask == dstAccessMask) {
    return Error::Success();
  }

  auto *commandBuffer = GetCommandBuffer();

  VkImageMemoryBarrier2 barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = currentLayout;
  barrier.newLayout = layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = GetAspectFlagsForFormat(format);
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  barrier.srcAccessMask = srcAccessMask;
  barrier.dstAccessMask = dstAccessMask;

  barrier.srcStageMask = sourceStage;
  barrier.dstStageMask = destinationStage;

  VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                       .imageMemoryBarrierCount = 1,
                       .pImageMemoryBarriers = &barrier};

  vkCmdPipelineBarrier2(commandBuffer, &dep);

  currentLayout = layout;

  return Error::Success();
}

auto Texture::SetFilter(VkFilter minFilter, VkFilter magFilter, // NOLINT
                        VkSamplerMipmapMode mipFilter) -> void {
  samplerDescription.minFilter = minFilter;
  samplerDescription.magFilter = magFilter;
  samplerDescription.mipmapMode = mipFilter;

  samplerDirty = true;
}

auto Texture::GetFilter() const
    -> std::tuple<VkFilter, VkFilter, VkSamplerMipmapMode> {
  return {samplerDescription.minFilter, samplerDescription.magFilter,
          samplerDescription.mipmapMode};
}

auto Texture::SetAnisotropy(float anisotropy) -> void {
  samplerDescription.anisotropyEnable = (anisotropy > 1.0F);
  samplerDescription.maxAnisotropy = anisotropy;

  samplerDirty = true;
}

auto Texture::GetAnisotropy() const -> float {
  return samplerDescription.maxAnisotropy;
}

auto Texture::SetWrapmode(VkSamplerAddressMode addressModeU,
                          VkSamplerAddressMode addressModeV,
                          VkSamplerAddressMode addressModeW) -> void {
  samplerDescription.addressModeU = addressModeU;
  samplerDescription.addressModeV = addressModeV;
  samplerDescription.addressModeW = addressModeW;

  samplerDirty = true;
}

auto Texture::GetWrap() const
    -> std::tuple<VkSamplerAddressMode, VkSamplerAddressMode,
                  VkSamplerAddressMode> {
  return {samplerDescription.addressModeU, samplerDescription.addressModeV,
          samplerDescription.addressModeW};
}

auto Texture::SetLodBias(float mipLodBias) -> void {
  samplerDescription.mipLodBias = mipLodBias;

  samplerDirty = true;
}

auto Texture::GetLodBias() const -> float {
  return samplerDescription.mipLodBias;
}

// NOLINTNEXTLINE
auto Texture::SetLodRange(float minLod, float maxLod) -> void {
  samplerDescription.minLod = minLod;
  samplerDescription.maxLod = maxLod;

  samplerDirty = true;
}

auto Texture::GetLodRange() const -> std::tuple<float, float> {
  return {samplerDescription.minLod, samplerDescription.maxLod};
}

auto Texture::SetDepthCompare(bool enable, VkCompareOp compareOp) -> void {
  samplerDescription.compareEnable = enable;
  samplerDescription.compareOp = compareOp;

  samplerDirty = true;
}

auto Texture::GetDepthCompare() const -> std::tuple<bool, VkCompareOp> {
  return {samplerDescription.compareEnable, samplerDescription.compareOp};
}

auto Texture::GetSampler(GraphicsContext &context) -> VkSampler {
  if (samplerDirty) {
    sampler = GetOrCreateSampler(context, samplerDescription);
    samplerDirty = false;

    return sampler;
  }

  return sampler;
}

auto Texture::SetPixels(GraphicsContext &context, Image::ImageData &imageData,
                        uint32_t mipLevel, uint32_t arrayLayer, // NOLINT
                        VkRect2D source,                        // NOLINT
                        VkOffset2D target) -> Error {
  if (source.extent.width > size.width || source.extent.height > size.height) {
    return Error::Create(
        "ImageData dimensions exceed texture dimensions in SetPixels.");
  }
  if (source.extent.width + target.x > size.width ||
      source.extent.height + target.y > size.height) {
    return Error::Create("Source width and target offset exceed texture "
                         "dimensions in SetPixels.");
  }
  if (target.x < 0 || target.y < 0) {
    return Error::Create(
        "Negative target offsets are not supported in SetPixels.");
  }
  if (source.extent.width > imageData.GetWidth() ||
      source.extent.height > imageData.GetHeight()) {
    return Error::Create(
        "Source rectangle exceeds ImageData dimensions in SetPixels.");
  }
  if (source.offset.x < 0 || source.offset.y < 0) {
    return Error::Create(
        "Negative source offsets are not supported in SetPixels.");
  }

  // Create staging buffer
  BufferCreationInfo bufferCreationInfo = {};
  bufferCreationInfo.size = imageData.GetSize();
  bufferCreationInfo.usage =
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreationInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  bufferCreationInfo.stagingBuffer = true;

  if (imageData.GetWidth() > size.width ||
      imageData.GetHeight() > size.height) {
    return Error::Create(
        "ImageData dimensions exceed texture dimensions in SetPixels.");
  }

  bufferCreationInfo.debugName = "Texture Staging Buffer for SetPixels";
  auto bufferResult = Buffer::Create(context, bufferCreationInfo);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }

  auto buffer = bufferResult.value();

  PrintAlways("Created staging buffer: {}", (void *)buffer->handle);

  // auto error = buffer->SetData(context, imageData.GetDataSpan());
  // also sets the buffer usage semaphore value

  auto rowSize =
      static_cast<size_t>(source.extent.width) * Format::GetSize(format);
  auto rowCount = static_cast<size_t>(source.extent.height);

  Graphics::Barrier::UpdateUsage(context, *this,
                                 Graphics::Barrier::ResourceState{
                                     .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                                     .access = VK_ACCESS_2_HOST_WRITE_BIT,
                                 });

  for (size_t row = 0; row < rowCount; ++row) {
    size_t sourceOffset =
        ((row + source.offset.y) * imageData.GetWidth() + source.offset.x) *
        Format::GetSize(format);

    auto rowSpan = // NOLINTNEXTLINE pointer arithmetic
        std::span<uint8_t>(imageData.GetDataPtr() + sourceOffset, rowSize);
    auto error = buffer->SetData(context, rowSpan, row * rowSize);

    if (Error::IsError(error)) {
      return error;
    }
  }

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = source.extent.width;
  region.bufferImageHeight = source.extent.height;
  region.imageSubresource.aspectMask = GetAspectFlagsForFormat(format);
  region.imageSubresource.mipLevel = mipLevel;
  region.imageSubresource.baseArrayLayer = arrayLayer;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {.x = target.x, .y = target.y, .z = 0};
  region.imageExtent = {
      .width = source.extent.width, .height = source.extent.height, .depth = 1};

  auto error = UseAsTransferDst(context);

  if (Error::IsError(error)) {
    return error;
  }

  auto *commandBuffer = GetCommandBuffer();

  vkCmdCopyBufferToImage(commandBuffer, buffer->handle, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  error = UseAsSampler(context, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

  if (Error::IsError(error)) {
    return error;
  }

  // TODO: Check lifetime
  buffer->MarkUse();
  MarkUse();

  return Error::Success();
}

auto Texture::SetPixels(GraphicsContext &context, Image::ImageData &imageData,
                        uint32_t mipLevel,
                        uint32_t arrayLayer) // NOLINT
    -> Error {
  VkRect2D source = {};
  source.offset = {.x = 0, .y = 0};
  source.extent = {.width = size.width, .height = size.height};
  VkOffset2D target = {0, 0};
  return SetPixels(context, imageData, mipLevel, arrayLayer, source, target);
}

auto Texture::SetPixels(GraphicsContext &context,
                        const std::span<const uint8_t> &data, size_t dataWidth,
                        size_t dataHeight, uint32_t mipLevel, // NOLINT
                        uint32_t arrayLayer, VkRect2D source, VkOffset2D target)
    -> Error {
  if (source.extent.width > size.width || source.extent.height > size.height) {
    return Error::Create(
        "Source rectangle dimensions exceed texture dimensions in SetPixels.");
  }
  if (source.extent.width + target.x > size.width ||
      source.extent.height + target.y > size.height) {
    return Error::Create("Source width and target offset exceed texture "
                         "dimensions in SetPixels.");
  }
  if (target.x < 0 || target.y < 0) {
    return Error::Create(
        "Negative target offsets are not supported in SetPixels.");
  }
  if (source.extent.width > dataWidth || source.extent.height > dataHeight) {
    return Error::Create(
        "Source rectangle exceeds ImageData dimensions in SetPixels.");
  }
  if (source.offset.x < 0 || source.offset.y < 0) {
    return Error::Create(
        "Negative source offsets are not supported in SetPixels.");
  }
  if (data.size() < dataWidth * dataHeight * Format::GetSize(format)) {
    return Error::Create("Data size is insufficient for specified dimensions.");
  }

  // Create staging buffer
  BufferCreationInfo bufferCreationInfo = {};
  bufferCreationInfo.size = data.size();
  bufferCreationInfo.usage =
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreationInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  bufferCreationInfo.stagingBuffer = true;

  if (dataWidth > size.width || dataHeight > size.height) {
    return Error::Create(
        "provided source dimensions exceed texture dimensions in SetPixels.");
  }
  bufferCreationInfo.debugName = "Texture Staging Buffer for SetPixels";

  auto bufferResult = Buffer::Create(context, bufferCreationInfo);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }

  auto rowSize =
      static_cast<size_t>(source.extent.width) * Format::GetSize(format);
  auto rowCount = static_cast<size_t>(source.extent.height);

  Graphics::Barrier::UpdateUsage(context, *this,
                                 Graphics::Barrier::ResourceState{
                                     .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                                     .access = VK_ACCESS_2_HOST_WRITE_BIT,
                                 });

  auto buffer = bufferResult.value();

  PrintAlways("Created staging buffer: {}", (void *)buffer->handle);

  for (size_t row = 0; row < rowCount; ++row) {
    size_t sourceOffset =
        ((row + source.offset.y) * dataWidth + source.offset.x) *
        Format::GetSize(format);

    auto rowSpan = // NOLINTNEXTLINE pointer arithmetic
        std::span<const uint8_t>(data.data() + sourceOffset, rowSize);
    auto error = buffer->SetData(context, rowSpan, row * rowSize);

    if (Error::IsError(error)) {
      return error;
    }
  }

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = source.extent.width;
  region.bufferImageHeight = source.extent.height;
  region.imageSubresource.aspectMask = GetAspectFlagsForFormat(format);
  region.imageSubresource.mipLevel = mipLevel;
  region.imageSubresource.baseArrayLayer = arrayLayer;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {.x = target.x, .y = target.y, .z = 0};
  region.imageExtent = {
      .width = source.extent.width, .height = source.extent.height, .depth = 1};

  auto error = UseAsTransferDst(context);

  if (Error::IsError(error)) {
    return error;
  }

  auto *commandBuffer = GetCommandBuffer();

  vkCmdCopyBufferToImage(commandBuffer, buffer->handle, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  error = UseAsSampler(context, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

  if (Error::IsError(error)) {
    return error;
  }

  buffer->MarkUse();
  MarkUse();

  PrintAlways("CPU Timestamp: {}", GetCPUTimelineSemaphoreValue());

  return Error::Success();
}

auto Texture::ScheduleDestroy() -> void {
  assert(!released);

  ScheduleDestruction(this);
  released = true;
}

struct VkFormatTextureTypeHash {
  auto operator()(const std::pair<VkFormat, TextureType> &key) const noexcept
      -> size_t {
    return std::hash<uint32_t>()(static_cast<uint32_t>(key.first)) ^
           (std::hash<uint8_t>()(static_cast<uint8_t>(key.second)) << 1U);
  }
};

std::unordered_map<std::pair<VkFormat, TextureType>, Ref<struct Texture>,
                   struct VkFormatTextureTypeHash>
    DefaultTextureCache; // NOLINT

auto UnloadModule() -> void { DefaultTextureCache.clear(); }

auto GetDefaultTexture(GraphicsContext &context, VkFormat format,
                       Graphics::Texture::TextureType textureType)
    -> Result<Ref<Graphics::Texture::Texture>> {

  auto key = std::make_pair(format, textureType);
  auto textureIterator = DefaultTextureCache.find(key);
  if (textureIterator != DefaultTextureCache.end()) {
    return textureIterator->second;
  }

  TextureCreationInfo texInfo = {};
  texInfo.width = 1;
  texInfo.height = 1;
  texInfo.depth = 1;
  texInfo.format = format;
  texInfo.usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                  static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  texInfo.debugName =
      "Default_Texture_" + Format::ImageFormatToString(format) + "_";
  switch (textureType) {
  case TextureType::DEFAULT:
    texInfo.debugName += "2D";
    break;
  case TextureType::CUBEMAP:
    texInfo.debugName += "Cubemap";
    break;
  case TextureType::VOLUME:
    texInfo.debugName += "Volume";
    break;
  case TextureType::ARRAY:
    texInfo.debugName += "Array";
    break;
  default:
    return Error::Unexpected("Unsupported texture type for default texture");
  }

  if (textureType == TextureType::CUBEMAP) {
    texInfo.depth = 6; // NOLINT
  }
  texInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  texInfo.mipmapCount = 1;

  Result<Ref<Texture>> result;
  switch (textureType) {
  case TextureType::DEFAULT:
    result = Graphics::Texture::Create2D(context, texInfo);
    break;
  case TextureType::CUBEMAP:
    result = Graphics::Texture::CreateCubeMap(context, texInfo);
    break;
  case TextureType::VOLUME:
    result = Graphics::Texture::CreateVolume(context, texInfo);
    break;
  case TextureType::ARRAY:
    result = Graphics::Texture::CreateArray(context, texInfo);
    break;
  default:
    return Error::Unexpected("Unsupported texture type for default texture");
  }

  if (Error::IsError(result)) {
    return result;
  }

  auto &texture = result.value();

  auto imageDataResult = Image::ImageData::Create(1, 1, format);

  if (Error::IsError(imageDataResult)) {
    return imageDataResult.error().AsUnexpected();
  }

  auto imageData = imageDataResult.value();
  imageData->SetColor(Math::Uvec2{0, 0},
                      Color(UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX));

  auto setPixelsResult = texture->SetPixels(context, *imageData);

  if (Error::IsError(setPixelsResult)) {
    return setPixelsResult.AsUnexpected();
  }

  DefaultTextureCache[key] = texture;

  return texture;
}

constexpr auto GetAccessFlagsForUsage(TextureUsage usage,
                                      VkImageLayout currentLayout)
    -> VkAccessFlags2 {
  switch (usage) {
  case TextureUsage::Sampler:
    return VK_ACCESS_2_SHADER_READ_BIT;
  case TextureUsage::Storage:
    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  case TextureUsage::Attachment:
    if (currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
      return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    } else if (currentLayout ==
               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
      return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
  case TextureUsage::TransferSrc:
    return VK_ACCESS_2_TRANSFER_READ_BIT;
  case TextureUsage::TransferDst:
    return VK_ACCESS_2_TRANSFER_WRITE_BIT;
  case TextureUsage::Unknown:
    return VK_ACCESS_2_NONE;
  default:
    PrintError("GetAccessFlagsForUsage: Unknown texture usage: {}",
               static_cast<int>(usage));
    return VK_ACCESS_2_NONE;
  }
}

constexpr auto GetRequiredTextureLayout(TextureUsage usage, VkFormat format)
    -> VkImageLayout {

#ifndef NDEBUG
  assert(
      usage != TextureUsage::Unknown &&
      "GetRequiredTextureLayout: TextureUsage::Unknown is not a valid usage.");
#endif

  switch (usage) {
  case TextureUsage::Sampler:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case TextureUsage::Storage:
    return VK_IMAGE_LAYOUT_GENERAL;
  case TextureUsage::Attachment:
    if (Image::IsDepthTexture(format) || Image::IsStencilTexture(format)) {
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else {
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  case TextureUsage::TransferSrc:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case TextureUsage::TransferDst:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case TextureUsage::Unknown:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  default:
    PrintError("GetRequiredTextureLayout: Unknown texture usage: {}",
               static_cast<int>(usage));
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

constexpr auto IsStageAllowed(VkPipelineStageFlags2 stage) -> bool {
  switch (stage) {
  case (VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT):
  case (VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT):
  case (VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT):
  case (VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT):
  case (VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT):
  case (VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT):
  case (VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT):
  case (VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT):
  case (VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT):
  case (VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT):
  case (VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT):
  case (VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT):
  case (VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT):
  case (VK_PIPELINE_STAGE_2_TRANSFER_BIT):
  case (VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT):
  case (VK_PIPELINE_STAGE_2_HOST_BIT):
  case (VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT):
  case (VK_PIPELINE_STAGE_2_COPY_BIT):
  case (VK_PIPELINE_STAGE_2_RESOLVE_BIT):
  case (VK_PIPELINE_STAGE_2_BLIT_BIT):
  case (VK_PIPELINE_STAGE_2_CLEAR_BIT):
    return true;
  default:
    PrintWarning("IsStageAllowed: Unsupported pipeline stage: {}",
                 static_cast<uint32_t>(stage));
    return false;
  }
}

auto Texture::UseAs(GraphicsContext &context, TextureUsage newUsage,
                    VkPipelineStageFlags2 stage) -> Error {

  if (newUsage == TextureUsage::Unknown) {
    return Error::Create("UseAs: TextureUsage::Unknown is not a valid usage.");
  }

  if (stage == VK_PIPELINE_STAGE_2_NONE) {
    return Error::Create(
        "UseAs: stage must be known when transitioning layouts.");
  }

  if (!IsStageAllowed(stage)) {
    return Error::Create(
        "UseAs: Unsupported pipeline stage for texture usage transition.");
  }

  auto layout = GetRequiredTextureLayout(newUsage, format);

  VkAccessFlags2 currentAccess =
      GetAccessFlagsForUsage(lastUsage, currentLayout);
  VkAccessFlags2 newAccess = GetAccessFlagsForUsage(newUsage, layout);

  if (lastUsage == TextureUsage::Unknown) {
    // First time usage, so we can skip the transition from UNDEFINED
    currentAccess = 0;
    lastPipelineStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  }

  auto result = TransitionLayout(context, layout, lastPipelineStage, stage,
                                 currentAccess, newAccess);

  lastUsage = newUsage;
  lastPipelineStage = stage;
  return result;
}

auto Texture::UseAsAttachment(GraphicsContext &context) -> Error {
  auto newPipelineStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  return UseAs(context, TextureUsage::Attachment, newPipelineStage);
}

auto Texture::UseAsSampler(GraphicsContext &context,
                           VkPipelineStageFlags2 stage) -> Error {
  return UseAs(context, TextureUsage::Sampler, stage);
}
auto Texture::UseAsTransferSrc(GraphicsContext &context) -> Error {
  return UseAs(context, TextureUsage::TransferSrc,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
}
auto Texture::UseAsTransferDst(GraphicsContext &context) -> Error {
  return UseAs(context, TextureUsage::TransferDst,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
}
auto Texture::UseAsStorage(GraphicsContext &context,
                           VkPipelineStageFlags2 stage) -> Error {
  return UseAs(context, TextureUsage::Storage, stage);
}

} // namespace Graphics::Texture
