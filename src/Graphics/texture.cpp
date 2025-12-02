#include "texture.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "sampler.hpp"
#include "stb/stb_image.h"
#include "tl/expected.hpp"
#include <array>
#include <cstdint>
#include <iostream>

#define VMA_VULKAN_VERSION 1004000
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <vma/vk_mem_alloc.h>

namespace Graphics::Texture {

auto StartSingleUseCommandBuffer(GraphicsContext &context)
    -> tl::expected<VkCommandBuffer, Error::Error> {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = GetRenderData(context, 0).pool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

  Error::Error error = Error::Create(
      vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  return commandBuffer;
}

auto EndSingleUseCommandBuffer(GraphicsContext &context,
                               VkCommandBuffer commandBuffer) -> Error::Error {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  Error::Error error = Error::Create(
      vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

  if (Error::IsError(error)) {
    return error;
  }
  vkQueueWaitIdle(context.graphicsQueue);

  // Cleanup
  VkCommandPool commandPool = GetRenderData(context, 0).pool;

  vkFreeCommandBuffers(context.device, commandPool, 1, &commandBuffer);
  vkDestroyCommandPool(context.device, commandPool, nullptr);

  return Error::Success();
}

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

auto Create2D(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error> {

  Texture texture = {};

  texture.size = VkExtent3D{info.width, info.height, 1};
  texture.format = info.format;
  texture.textureType = TextureType::DEFAULT;
  texture.mipmapcount = info.mipmapCount;
  texture.usage = info.usage;
  texture.arrayLayers = 1;
  texture.samplerDirty = true;

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

  auto error =
      Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo, &allocInfo,
                                   &texture.image, &texture.memory, nullptr));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  context.runtimeInfo.textureCount++;

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  error = Error::Create(
      vkCreateImageView(context.device, &viewInfo, nullptr, &texture.view));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  VmaAllocationInfo memRequirements;
  vmaGetAllocationInfo(context.vmaAllocator, texture.memory, &memRequirements);
  texture.sizeInBytes = memRequirements.size;

  return texture;
}

auto FromSwapchainTexture(GraphicsContext &context, VkImage swapchainImage,
                          VkFormat format, uint32_t width, uint32_t height)
    -> tl::expected<Texture, Error::Error> {

  Texture texture = {};

  texture.image = swapchainImage;
  texture.format = format;
  texture.size = VkExtent3D{width, height, 1};
  texture.textureType = TextureType::DEFAULT;
  texture.mipmapcount = 1;
  texture.arrayLayers = 1;
  texture.samplerDirty = true;

  VkSurfaceCapabilitiesKHR surfaceCapabilities;

  auto error = Error::Create(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      context.physicalDevice, context.surface, &surfaceCapabilities));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  texture.usage = surfaceCapabilities.supportedUsageFlags;

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  error = Error::Create(
      vkCreateImageView(context.device, &viewInfo, nullptr, &texture.view));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  return texture;
}

auto CreateCubeMap(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error> {

  if (info.width != info.height) {
    return tl::unexpected(
        Error::Create("Cube map textures must have equal width and height."));
  }
  const int CubeFaceCount = 6;

  Texture texture = {};

  texture.size = VkExtent3D{info.width, info.width, 1};
  texture.format = info.format;
  texture.textureType = TextureType::CUBEMAP;
  texture.mipmapcount = info.mipmapCount;
  texture.usage = info.usage;
  texture.arrayLayers = CubeFaceCount;
  texture.samplerDirty = true;

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
  Error::Error error =
      Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo, &allocInfo,
                                   &texture.image, &texture.memory, nullptr));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  context.runtimeInfo.textureCount++;

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = CubeFaceCount;

  error = Error::Create(
      vkCreateImageView(context.device, &viewInfo, nullptr, &texture.view));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  VmaAllocationInfo memRequirements;
  vmaGetAllocationInfo(context.vmaAllocator, texture.memory, &memRequirements);
  texture.sizeInBytes = memRequirements.size;

  return texture;
}

auto CreateVolume(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error> {

  Texture texture = {};

  texture.size = VkExtent3D{info.width, info.height, info.depth};
  texture.format = info.format;
  texture.textureType = TextureType::VOLUME;
  texture.mipmapcount = info.mipmapCount;
  texture.usage = info.usage;
  texture.arrayLayers = 1;
  texture.samplerDirty = true;

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

  Error::Error error =
      Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo, &allocInfo,
                                   &texture.image, &texture.memory, nullptr));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  context.runtimeInfo.textureCount++;

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  error = Error::Create(
      vkCreateImageView(context.device, &viewInfo, nullptr, &texture.view));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  VmaAllocationInfo memRequirements;
  vmaGetAllocationInfo(context.vmaAllocator, texture.memory, &memRequirements);
  texture.sizeInBytes = memRequirements.size;

  return texture;
}

auto CreateArray(GraphicsContext &context, TextureCreationInfo info)
    -> tl::expected<Texture, Error::Error> {

  Texture texture = {};

  texture.size = VkExtent3D{info.width, info.height, info.depth};
  texture.format = info.format;
  texture.textureType = TextureType::ARRAY;
  texture.mipmapcount = info.mipmapCount;
  texture.usage = info.usage;
  texture.arrayLayers = info.depth;
  texture.samplerDirty = true;

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

  Error::Error error =
      Error::Create(vmaCreateImage(context.vmaAllocator, &imageInfo, &allocInfo,
                                   &texture.image, &texture.memory, nullptr));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  context.runtimeInfo.textureCount++;

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = info.depth;

  error = Error::Create(
      vkCreateImageView(context.device, &viewInfo, nullptr, &texture.view));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  VmaAllocationInfo memRequirements;
  vmaGetAllocationInfo(context.vmaAllocator, texture.memory, &memRequirements);
  texture.sizeInBytes = memRequirements.size;

  return texture;
}

void Destroy(GraphicsContext &context, Texture *texture) {
  if (texture->view != VK_NULL_HANDLE) {
    vkDestroyImageView(context.device, texture->view, nullptr);
    texture->view = VK_NULL_HANDLE;
  }

  if (texture->image != VK_NULL_HANDLE) {
    vmaDestroyImage(context.vmaAllocator, texture->image, texture->memory);
    texture->image = VK_NULL_HANDLE;
    texture->memory = VK_NULL_HANDLE;
  }

  context.runtimeInfo.textureCount--;
}

auto LoadFromFile(GraphicsContext &context, const char *path)
    -> tl::expected<Texture, Error::Error> {
  int texWidth = 0;
  int texHeight = 0;
  int texChannels = 0;
  stbi_uc *pixels =
      stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  if (pixels == nullptr) {
    return tl::unexpected(
        Error::Create("Failed to load texture image from file."));
  }

  int imageSize = (texWidth * texHeight * 4);

  // Create staging buffer
  VkBuffer stagingBuffer = nullptr;
  VmaAllocation stagingBufferMemory = nullptr;

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = imageSize;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

  Error::Error error = Error::Create(
      vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                      &stagingBuffer, &stagingBufferMemory, nullptr));

  // Copy pixel data to staging buffer
  void *data = nullptr;
  vmaMapMemory(context.vmaAllocator, stagingBufferMemory, &data);
  memcpy(data, pixels, imageSize);
  vmaUnmapMemory(context.vmaAllocator, stagingBufferMemory);

  stbi_image_free(pixels);

  TextureCreationInfo texInfo = {};
  texInfo.width = texWidth;
  texInfo.height = texHeight;
  texInfo.depth = 1;
  texInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  texInfo.usage = static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
                  static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT);

  // Create texture image
  auto result = Create2D(context, texInfo);

  if (Error::IsError(error)) {
    return result;
  }

  Texture texture = result.value();

  // Transition image layout and copy data from staging buffer
  error = (TransitionLayout(context, &texture, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  error = (CopyBufferToImage(context, stagingBuffer, &texture));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  error =
      (TransitionLayout(context, &texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  // Clean up staging buffer
  vmaDestroyBuffer(context.vmaAllocator, stagingBuffer, stagingBufferMemory);

  return texture;
}

auto TransitionLayout(GraphicsContext &context, Texture *texture,
                      VkImageLayout oldLayout, VkImageLayout newLayout)
    -> Error::Error {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = GetRenderData(context, 0).pool;
  allocInfo.commandBufferCount = 1;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture->image;
  barrier.subresourceRange.aspectMask =
      GetAspectFlagsForFormat(texture->format);
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags sourceStage = 0;
  VkPipelineStageFlags destinationStage = 0;

  // Determine source and destination access masks and pipeline stages
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    return Error::Create("Unsupported layout transition.");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  vkEndCommandBuffer(commandBuffer);
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context.graphicsQueue);
  vkFreeCommandBuffers(context.device, GetRenderData(context, 0).pool, 1,
                       &commandBuffer);

  return Error::Success();
}

auto CopyBufferToImage(GraphicsContext &context, VkBuffer buffer,
                       Texture *texture) -> Error::Error {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = GetRenderData(context, 0).pool;
  allocInfo.commandBufferCount = 1;
  VkCommandBuffer commandBuffer = nullptr;
  vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = GetAspectFlagsForFormat(texture->format);
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {.x = 0, .y = 0, .z = 0};
  region.imageExtent = texture->size;

  vkCmdCopyBufferToImage(commandBuffer, buffer, texture->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  vkEndCommandBuffer(commandBuffer);
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context.graphicsQueue);
  vkFreeCommandBuffers(context.device, GetRenderData(context, 0).pool, 1,
                       &commandBuffer);

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

auto Texture::GetWrapmode() const
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

    std::cout << "Created new sampler for texture.\n";

    return sampler;
  }

  std::cout << "Reusing existing sampler for texture.\n";

  return sampler;
}

struct VkFormatTextureTypeHash {
  auto operator()(const std::pair<VkFormat, TextureType> &key) const noexcept
      -> size_t {
    return std::hash<uint32_t>()(static_cast<uint32_t>(key.first)) ^
           (std::hash<uint8_t>()(static_cast<uint8_t>(key.second)) << 1U);
  }
};

auto GetDefaultTexture(GraphicsContext &context, VkFormat format,
                       Graphics::Texture::TextureType textureType)
    -> tl::expected<Graphics::Texture::Texture, Error::Error> {
  static std::unordered_map<std::pair<VkFormat, TextureType>, Texture,
                            VkFormatTextureTypeHash>
      textureCache;

  auto key = std::make_pair(format, textureType);
  auto textureIterator = textureCache.find(key);
  if (textureIterator != textureCache.end()) {
    return textureIterator->second;
  }

  TextureCreationInfo texInfo = {};
  texInfo.width = 1;
  texInfo.height = 1;
  texInfo.depth = 1;
  texInfo.format = format;
  texInfo.usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                  static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  if (textureType == TextureType::CUBEMAP) {
    texInfo.depth = 6; // NOLINT
  }
  texInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  texInfo.mipmapCount = 1;

  tl::expected<Texture, Error::Error> result;
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
    return tl::unexpected(
        Error::Create("Unsupported texture type for default texture."));
  }

  if (Error::IsError(result)) {
    return result;
  }

  Texture &texture = result.value();

  // Fill texture with 1x1 of opaque white pixel data
  std::array<uint8_t, 4> whitePixel = {UINT8_MAX, UINT8_MAX, UINT8_MAX,
                                       UINT8_MAX};
  VkBuffer stagingBuffer = nullptr;
  VmaAllocation stagingBufferMemory = nullptr;
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = whitePixel.size();
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  Error::Error error = Error::Create(
      vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                      &stagingBuffer, &stagingBufferMemory, nullptr));
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }
  void *data = nullptr;
  vmaMapMemory(context.vmaAllocator, stagingBufferMemory, &data);
  memcpy(data, whitePixel.data(), whitePixel.size());
  vmaUnmapMemory(context.vmaAllocator, stagingBufferMemory);
  error = Graphics::Texture::TransitionLayout(
      context, &texture, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }
  error =
      Graphics::Texture::CopyBufferToImage(context, stagingBuffer, &texture);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }
  error = Graphics::Texture::TransitionLayout(
      context, &texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }
  vmaDestroyBuffer(context.vmaAllocator, stagingBuffer, stagingBufferMemory);

  textureCache[key] = texture;

  return texture;
}
} // namespace Graphics::Texture
