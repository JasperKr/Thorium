#include "texture.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/renderThread.hpp"
#include "Graphics/resource.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/image.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include "sampler.hpp"
#include "stb/stb_image.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <mutex>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#define VMA_VULKAN_VERSION 1004000

#include "vulkan/vulkan_core.h"

namespace Graphics {

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

  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  VkDebugUtilsObjectNameInfoEXT nameInfo = {};
  nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
  nameInfo.objectHandle = static_cast<uint64_t>(
      reinterpret_cast<uintptr_t>(texture->image)), // NOLINT
      nameInfo.pObjectName = debugName.c_str();

  CHECK_ERR(
      Error::Create(vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo)));

  vmaSetAllocationName(context.vmaAllocator, texture->memory,
                       debugName.c_str());

  return Error::Success();
}

auto Texture::Create(const GraphicsContext &context,
                     const TextureCreationInfo &info) -> Result<Ref<Texture>> {
  ZoneScoped;

  if (((info.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0U) &&
      (info.format == VK_FORMAT_R8G8B8A8_SRGB ||
       info.format == VK_FORMAT_B8G8R8A8_SRGB)) {
    return Error::Unexpected("SRGB formats are not supported for storage "
                             "usage.");
  }

  if (info.size.width == 0 || info.size.height == 0 || info.size.depth == 0) {
    return Error::Unexpected("Texture dimensions must be greater than 0.");
  }

  if (info.arrayLayers == 0) {
    return Error::Unexpected("Texture array layers must be greater than 0.");
  }

  if (info.mipmapCount == 0) {
    return Error::Unexpected("Texture mipmap count must be greater than 0.");
  }

  if (info.format == VK_FORMAT_UNDEFINED) {
    return Error::Unexpected("Texture format must be defined.");
  }

  if ((info.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0U &&
      (info.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0U) {
    return Error::Unexpected(
        "Texture usage must include at least sampled or storage usage.");
  }

  Ref<Texture> texture = Ref<Texture>::Make();

  texture->size = info.size;
  texture->format = info.format;
  texture->textureType = info.textureType;
  texture->mipmapcount = info.mipmapCount;
  texture->usage = info.usage;
  texture->arrayLayers = info.arrayLayers;
  texture->samplerDirty = true;
  texture->debugName =
      info.debugName.empty() ? "Unnamed Texture" : info.debugName;

  const auto &config = Threading::GetGraphicsConfiguration();

  texture->samplerDescription = config.defaultSamplerDescription;

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = info.size;
  imageInfo.mipLevels = info.mipmapCount;
  imageInfo.arrayLayers = info.arrayLayers;
  imageInfo.format = info.format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = info.usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (imageInfo.mipLevels > 1) {
    imageInfo.usage |= static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  allocInfo.requiredFlags = 0;
  allocInfo.preferredFlags = 0;

  VmaAllocationInfo memRequirements;

  {
    std::scoped_lock<std::mutex, std::mutex> lock(
        Graphics::GraphicsContext::mutexes.device,
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    CHECK_NEW_ERR(vmaCreateImage(context.vmaAllocator, &imageInfo, &allocInfo,
                                 &texture->image, &texture->memory,
                                 &memRequirements));
  }

  // ignore set debug name error, since it's not critical
  auto err = SetDebugName(info.debugName, texture.get(), context);

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = info.format;
  viewInfo.subresourceRange.aspectMask = GetAspectFlagsForFormat(info.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = info.mipmapCount;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = info.arrayLayers;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateImageView(context.device, &viewInfo,
                                    GetAllocationCallbacks(), &texture->view));
  }

  texture->sizeInBytes = memRequirements.size;
  Texture::TotalAllocatedMemory += texture->sizeInBytes;

  return texture;
}

auto Texture::Create(const GraphicsContext &context, const Texture *texture,
                     VkImageSubresourceRange range) -> Result<Ref<Texture>> {
  ZoneScoped;

  // Check vadility of the provided range against the parent texture

  if (range.baseMipLevel + range.levelCount >= texture->mipmapcount) {
    return Error::Unexpected(
        "Base mip level is out of range of the parent texture.");
  }

  if (range.baseArrayLayer + range.layerCount >= texture->arrayLayers) {
    return Error::Unexpected(
        "Base array layer is out of range of the parent texture.");
  }
  range.aspectMask = GetAspectFlagsForFormat(texture->format);

  Ref<Texture> textureView = Ref<Texture>::Make();

  textureView->size = texture->size;
  textureView->format = texture->format;
  textureView->textureType = texture->textureType;
  textureView->mipmapcount = range.levelCount;
  textureView->usage = texture->usage;
  textureView->arrayLayers = range.layerCount;
  textureView->samplerDirty = true;
  textureView->debugName = texture->debugName + "_view";
  textureView->isView = true;

  const auto &config = Threading::GetGraphicsConfiguration();

  textureView->samplerDescription = config.defaultSamplerDescription;
  textureView->image = texture->image;
  textureView->memory = texture->memory;
  textureView->currentLayout = texture->currentLayout;
  textureView->sizeInBytes = texture->sizeInBytes;
  textureView->isSwapchainView = texture->isSwapchainView;

  // ignore set debug name error, since it's not critical
  auto err = SetDebugName(textureView->debugName, textureView.get(), context);

  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = textureView->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = textureView->format;
  viewInfo.subresourceRange = range;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateImageView(context.device, &viewInfo,
                                    GetAllocationCallbacks(),
                                    &textureView->view));
  }

  texture->retain();

  return textureView;
}

auto Texture::FromSwapchain(const GraphicsContext &context,
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

  texture->usage = context.surfaceInfo.capabilities.supportedUsageFlags;
  texture->view = swapchainImageView;
  texture->lastUsage = TextureUsage::Swapchain;

  return texture;
}

auto Texture::FromFile(GraphicsContext &context, const char *path,
                       VkImageUsageFlags usage, TextureMipmapOption mipmaps)
    -> Result<Ref<Texture>> {
  auto filedata = CHECK_RES(Filesystem::ReadFile(path));

  auto dataSpan =
      std::span<uint8_t>(filedata.data(), static_cast<size_t>(filedata.size()));

  auto imageData = CHECK_RES(Image::ImageData::Create(dataSpan));

  int mipmapCount = 1;

  if (mipmaps != TextureMipmapOption::None) {
    mipmapCount = static_cast<int>(
        Image::GetMipmapCount(imageData->GetWidth(), imageData->GetHeight()));
  }

  auto texture = CHECK_RES(
      Create(context, TextureCreationInfo{
                          .size = imageData->GetDimensions(),
                          .format = imageData->GetFormat(),
                          .usage = usage | static_cast<uint32_t>(
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                          .mipmapCount = mipmapCount,
                          .debugName = "Image_" + std::string(path),
                          .textureType = TextureType::DEFAULT,
                      }));

  CHECK_ERR(texture->SetPixels(context, *imageData));

  if (mipmaps == TextureMipmapOption::Init &&
      !Image::IsCompressedTexture(imageData->GetFormat())) {
    CHECK_ERR(texture->GenerateMipmaps(context));
  }

  return texture;
}

// texture 2D From ImageData
auto Texture::FromMemory(GraphicsContext &context, Image::ImageData &imageData,
                         VkImageUsageFlags usage, TextureMipmapOption mipmaps)
    -> Result<Ref<Texture>> {

  if (Image::IsCompressedTexture(imageData.GetFormat()) &&
      mipmaps != TextureMipmapOption::None) {
    // Mipmaps for compressed textures must be provided manually
    mipmaps = TextureMipmapOption::Manual;
  }

  int mipmapCount = 1;
  if (mipmaps != TextureMipmapOption::None) {
    mipmapCount = static_cast<int>(
        Image::GetMipmapCount(imageData.GetWidth(), imageData.GetHeight()));
  }

  auto texture = CHECK_RES(
      Create(context, TextureCreationInfo{
                          .size = imageData.GetDimensions(),
                          .format = imageData.GetFormat(),
                          .usage = usage | static_cast<uint32_t>(
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                          .mipmapCount = mipmapCount,
                          .debugName = "Image_ImageData",
                      }));

  CHECK_ERR(texture->SetPixels(context, imageData, 0, 0));

  if (mipmaps == TextureMipmapOption::Init) {
    CHECK_ERR(texture->GenerateMipmaps(context));
  }

  return texture;
}

auto Texture::FromMemory(GraphicsContext &context,
                         const Image::CompressedImageData &compressedData,
                         VkImageUsageFlags usage, TextureMipmapOption mipmaps)
    -> Result<Ref<Texture>> {
  if (mipmaps == TextureMipmapOption::Init) {
    return Error::Unexpected(
        "Automatic mipmap generation is not supported for compressed textures. "
        "Please provide mipmaps manually.");
  }

  auto texture = CHECK_RES(Create(
      context,
      TextureCreationInfo{
          .size = {compressedData.GetWidth(), compressedData.GetHeight(), 1},
          .format = compressedData.GetFormat(),
          .usage =
              usage | static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
          .mipmapCount = compressedData.GetMipmapCount(),
          .debugName = "Image_CompressedImageData",
          .textureType = TextureType::DEFAULT,
      }));

  size_t paddedSize = 0U;

  int mipLevelCount = compressedData.GetMipmapCount();

  if (mipmaps == TextureMipmapOption::None) {
    mipLevelCount = 1;
  }

  auto blockSize = Graphics::Format::GetSize(compressedData.GetFormat());

  for (int mip = 0; mip < mipLevelCount; ++mip) {
    auto size = Image::GetDimensions(compressedData.GetDimensions(), mip);
    auto blocksX = (size.width + 3) / 4;
    auto blocksY = (size.height + 3) / 4;
    size_t levelSize = static_cast<size_t>(blocksX) * blocksY * blockSize;

    // Align each mip level to the device's optimal buffer copy offset alignment
    auto limits =
        GetCurrentGraphicsContext()
            ->deviceProperties.limits.optimalBufferCopyOffsetAlignment;

    paddedSize += Utils::AlignUp(levelSize, limits);
  }

  // Create staging buffer
  BufferCreationInfo bufferCreationInfo = {};
  bufferCreationInfo.size = paddedSize;
  bufferCreationInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreationInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  bufferCreationInfo.stagingBuffer = true;
  bufferCreationInfo.persistentMapping = false;

  bufferCreationInfo.debugName = "Texture Staging Buffer for SetPixels";
  auto buffer = CHECK_RES(Buffer::Create(context, bufferCreationInfo));

  Graphics::Barrier::UpdateUsage(context, *texture.get(),
                                 Graphics::Barrier::ResourceState{
                                     .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                                     .access = VK_ACCESS_2_HOST_WRITE_BIT,
                                 });

  size_t offset = 0;

  for (int mip = 0; mip < mipLevelCount; ++mip) {
    auto size = Image::GetDimensions(compressedData.GetDimensions(), mip);
    auto blocksX = (size.width + 3) / 4;
    auto blocksY = (size.height + 3) / 4;
    size_t levelSize = static_cast<size_t>(blocksX) * blocksY * blockSize;

    CHECK_ERR(buffer->SetData(context, compressedData.GetSpan().subspan(offset),
                              offset, levelSize));

    auto limits =
        GetCurrentGraphicsContext()
            ->deviceProperties.limits.optimalBufferCopyOffsetAlignment;

    offset += Utils::AlignUp(levelSize, limits);
  }

  std::vector<VkBufferImageCopy> copyRegions;
  copyRegions.reserve(mipLevelCount);
  offset = 0;

  for (int mip = 0; mip < mipLevelCount; ++mip) {
    VkBufferImageCopy region = {};
    region.bufferOffset = offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask =
        GetAspectFlagsForFormat(compressedData.GetFormat());
    region.imageSubresource.mipLevel = static_cast<uint32_t>(mip);
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {.x = 0, .y = 0, .z = 0};

    auto size = Image::GetDimensions(compressedData.GetDimensions(), mip);

    region.imageExtent = {
        .width = size.width, .height = size.height, .depth = 1};

    copyRegions.emplace_back(region);

    auto blocksX = (size.width + 3) / 4;
    auto blocksY = (size.height + 3) / 4;

    auto limits =
        GetCurrentGraphicsContext()
            ->deviceProperties.limits.optimalBufferCopyOffsetAlignment;

    offset += Utils::AlignUp(
        static_cast<VkDeviceSize>(blocksX) * blocksY * blockSize, limits);
  }

  CHECK_ERR(texture->UseAsTransferDst(context));

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for SetPixels.");
  }

  DynamicRendering::EndRendering(context);

  vkCmdCopyBufferToImage(commandBuffer, buffer->handle, texture->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(copyRegions.size()),
                         copyRegions.data());

  // TODO: Check lifetime
  buffer->MarkUse();
  texture->MarkUse();

  return texture;
}

// texture 3D/Array/Cubemap From array of ImageData slices
auto Texture::FromMemory(GraphicsContext &context,
                         const std::vector<Image::ImageData *> &slices,
                         TextureType type, VkImageUsageFlags usage,
                         TextureMipmapOption mipmaps) -> Result<Ref<Texture>> {
  if (slices.empty()) {
    return Error::Unexpected("No image slices provided.");
  }

  uint32_t width = slices[0]->GetWidth();
  uint32_t height = slices[0]->GetHeight();

  Ref<Texture> texture;

  int mipmapCount = 1;

  switch (type) {
  case TextureType::CUBEMAP: {
    if (slices.size() != 6) { // NOLINT
      return Error::Unexpected("Cubemap textures require 6 image slices.");
    }

    if (mipmaps != TextureMipmapOption::None) {
      mipmapCount = static_cast<int>(Image::GetMipmapCount(width, height));
    }

    auto cubeMapTexture = Create(
        context, TextureCreationInfo{
                     .size = VkExtent3D{width, height, 1},
                     .arrayLayers = 6, // NOLINT
                     .format = slices[0]->GetFormat(),
                     .usage = usage | static_cast<uint32_t>(
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                     .mipmapCount = mipmapCount,
                     .textureType = TextureType::CUBEMAP,
                 });

    if (Error::IsError(cubeMapTexture)) {
      return cubeMapTexture.error();
    }

    texture = cubeMapTexture.value();
    break;
  }
  case TextureType::ARRAY: {
    if (mipmaps != TextureMipmapOption::None) {
      mipmapCount = static_cast<int>(Image::GetMipmapCount(width, height));
    }

    auto arrayTexture = Create(
        context, TextureCreationInfo{
                     .size = VkExtent3D{width, height, 1},
                     .arrayLayers = static_cast<uint32_t>(slices.size()),
                     .format = slices[0]->GetFormat(),
                     .usage = usage | static_cast<uint32_t>(
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                     .mipmapCount = mipmapCount,
                     .textureType = TextureType::ARRAY,
                 });

    if (Error::IsError(arrayTexture)) {
      return arrayTexture.error();
    }

    texture = arrayTexture.value();
    break;
  }
  case TextureType::VOLUME: {
    if (mipmaps != TextureMipmapOption::None) {
      mipmapCount =
          static_cast<int>(Image::GetMipmapCount(width, height, slices.size()));
    }

    auto volumeTexture = Create(
        context, TextureCreationInfo{
                     .size = VkExtent3D{width, height,
                                        static_cast<uint32_t>(slices.size())},
                     .format = slices[0]->GetFormat(),
                     .usage = usage | static_cast<uint32_t>(
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                     .mipmapCount = mipmapCount,
                     .textureType = TextureType::VOLUME,
                 });

    if (Error::IsError(volumeTexture)) {
      return volumeTexture.error();
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
        texture->SetPixels(context, *slices[i], 0, static_cast<uint32_t>(i));
    if (Error::IsError(result)) {
      return result;
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
  case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    return "PRESENT_SRC_KHR";
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

auto Texture::TransitionLayout(const GraphicsContext &context,
                               VkImageLayout layout,
                               VkPipelineStageFlags2 sourceStage, // NOLINT
                               VkPipelineStageFlags2 destinationStage,
                               VkAccessFlags2 srcAccessMask, // NOLINT
                               VkAccessFlags2 dstAccessMask,
                               VkImageSubresourceRange range) -> Error {

  if (layout == VK_IMAGE_LAYOUT_UNDEFINED) {
    return Error::Create("Cannot transition to UNDEFINED layout.");
  }

  if (currentLayout == layout && sourceStage == destinationStage &&
      srcAccessMask == dstAccessMask) {
    return Error::Success();
  }

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for layout transition.");
  }

  VkImageMemoryBarrier2 barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = currentLayout;
  barrier.newLayout = layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = range;
  barrier.subresourceRange.aspectMask = GetAspectFlagsForFormat(format);

  barrier.srcAccessMask = srcAccessMask;
  barrier.dstAccessMask = dstAccessMask;

  barrier.srcStageMask = sourceStage;
  barrier.dstStageMask = destinationStage;

  VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                       .imageMemoryBarrierCount = 1,
                       .pImageMemoryBarriers = &barrier};

  DynamicRendering::EndRendering(context);
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

auto Texture::SetBorderColor(VkBorderColor borderColor) -> void {
  samplerDescription.borderColor = borderColor;

  samplerDirty = true;
}

auto Texture::GetBorderColor() const -> VkBorderColor {
  return samplerDescription.borderColor;
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

auto Texture::GetSampler(const GraphicsContext &context) -> VkSampler {
  if (samplerDirty) {
    sampler = GetOrCreateSampler(context, samplerDescription);
    samplerDirty = false;

    return sampler;
  }

  return sampler;
}

inline auto WriteSimplifiedPixelData(const Ref<Texture> &texture,
                                     const GraphicsContext &context,
                                     const std::span<const uint8_t> &data,
                                     uint32_t mipLevel, // NOLINT
                                     uint32_t arrayLayer) {
  // Create staging buffer
  BufferCreationInfo bufferCreationInfo = {};
  bufferCreationInfo.size = data.size();
  bufferCreationInfo.usage =
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreationInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  bufferCreationInfo.stagingBuffer = true;

  bufferCreationInfo.debugName =
      "Texture Staging Buffer for SetPixels (Compressed Format)";

  auto buffer = CHECK_RES(Buffer::Create(context, bufferCreationInfo));

  CHECK_ERR(buffer->SetData(context, data, 0));

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = GetAspectFlagsForFormat(texture->format);
  region.imageSubresource.mipLevel = mipLevel;
  region.imageSubresource.baseArrayLayer = arrayLayer;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {.x = 0, .y = 0, .z = 0};
  region.imageExtent = texture->size;

  CHECK_ERR(texture->UseAsTransferDst(context));

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for SetPixels.");
  }

  DynamicRendering::EndRendering(context);

  vkCmdCopyBufferToImage(commandBuffer, buffer->handle, texture->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  CHECK_ERR(
      texture->UseAsSampler(context, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT));

  buffer->MarkUse();
  texture->MarkUse();

  return Error::Success();
}

// Copies data from a 1D/2D/3D region from the provided data to the texture
// sourceSize is the dimensions of the underlying data
// sourceOffset is the offset within the data to start copying from
// target is the offset within the texture to copy to
// targetSize is the size of the region to copy within the texture
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Texture::SetPixels(const GraphicsContext &context,
                        const std::span<const uint8_t> &data,
                        uint32_t mipLevel, // NOLINT
                        uint32_t arrayLayer, VkExtent3D sourceSize,
                        VkOffset3D sourceOffset, // NOLINT
                        VkOffset3D target, VkExtent3D targetSize) -> Error {
  ZoneScoped;

  if (targetSize.width == size.width && targetSize.height == size.height &&
      targetSize.depth == size.depth) {
    // Fast path for full texture updates
    return WriteSimplifiedPixelData(Ref<Texture>(this), context, data, mipLevel,
                                    arrayLayer);
  }

  if (Image::IsCompressedTexture(format)) {
    return Error::Create("SetPixels with region copy is not supported for "
                         "compressed texture formats.");
  }

#ifndef NDEBUG
  if (sourceSize.width == 0 || sourceSize.height == 0 ||
      sourceSize.depth == 0) {
    return Error::Create(
        "Source size dimensions must be greater than zero in SetPixels.");
  }

  if (sourceSize.width < targetSize.width ||
      sourceSize.height < targetSize.height ||
      sourceSize.depth < targetSize.depth) {
    return Error::Create("Source size must be greater than or equal to target "
                         "size in SetPixels.");
  }

  if (target.x + targetSize.width > size.width ||
      target.y + targetSize.height > size.height ||
      target.z + targetSize.depth > size.depth) {
    return Error::Create(
        "Target dimensions exceed texture dimensions in SetPixels.");
  }
  if (target.x < 0 || target.y < 0 || target.z < 0) {
    return Error::Create(
        "Negative target offsets are not supported in SetPixels.");
  }
  if (sourceOffset.x < 0 || sourceOffset.y < 0 || sourceOffset.z < 0) {
    return Error::Create(
        "Negative source offsets are not supported in SetPixels.");
  }
  if (sourceOffset.x + targetSize.width > sourceSize.width ||
      sourceOffset.y + targetSize.height > sourceSize.height ||
      sourceOffset.z + targetSize.depth > sourceSize.depth) {
    return Error::Create(
        "Source offset and target size exceed source dimensions in SetPixels.");
  }
#endif

  auto uploadSize = targetSize.width * targetSize.height * targetSize.depth *
                    Format::GetSize(format);

  if (uploadSize > data.size()) {
    return Error::Create("Provided data size is smaller than expected for "
                         "given source dimensions and format in SetPixels.");
  }

  // Create staging buffer
  BufferCreationInfo bufferCreationInfo = {};
  bufferCreationInfo.size = uploadSize;
  bufferCreationInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreationInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  bufferCreationInfo.stagingBuffer = true;
  bufferCreationInfo.persistentMapping = false;

  bufferCreationInfo.debugName = "Texture Staging Buffer for SetPixels";
  auto buffer = CHECK_RES(Buffer::Create(context, bufferCreationInfo));

  Graphics::Barrier::UpdateUsage(context, *this,
                                 Graphics::Barrier::ResourceState{
                                     .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                                     .access = VK_ACCESS_2_HOST_WRITE_BIT,
                                 });

  std::vector<uint8_t> tempBuffer(uploadSize);
  auto formatSize = Format::GetSize(format);
  auto *dstPtr = tempBuffer.data();
  const auto *srcPtr = data.data();

  auto copySize = targetSize.width * formatSize;
  auto Xoffset = sourceOffset.x * formatSize;

  for (size_t zSlice = 0; zSlice < targetSize.depth; ++zSlice) {
    auto Zoffset = (sourceOffset.z + zSlice) * sourceSize.width *
                   sourceSize.height * formatSize;

    for (size_t row = 0; row < targetSize.height; ++row) {
      auto Yoffset = (sourceOffset.y + row) * sourceSize.width * formatSize;
      auto srcOffset = Xoffset + Yoffset + Zoffset;

      auto dstOffset =
          ((zSlice * targetSize.height + row) * targetSize.width) * formatSize;

      assert(srcOffset + copySize <= data.size());
      assert(dstOffset + copySize <= tempBuffer.size());

      // NOLINTNEXTLINE
      std::memcpy(dstPtr + dstOffset, srcPtr + srcOffset, copySize);
    }
  }

  CHECK_ERR(buffer->SetData(context, tempBuffer));

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = targetSize.width;
  region.bufferImageHeight = targetSize.height;
  region.imageSubresource.aspectMask = GetAspectFlagsForFormat(format);
  region.imageSubresource.mipLevel = mipLevel;
  region.imageSubresource.baseArrayLayer = arrayLayer;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = target;
  region.imageExtent = targetSize;

  CHECK_ERR(UseAsTransferDst(context));

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for SetPixels.");
  }

  DynamicRendering::EndRendering(context);

  vkCmdCopyBufferToImage(commandBuffer, buffer->handle, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // TODO: Check lifetime
  buffer->MarkUse();
  MarkUse();

  return Error::Success();
}

auto Texture::SetPixels(const GraphicsContext &context,
                        Image::ImageData &imageData, uint32_t mipLevel,
                        uint32_t arrayLayer) // NOLINT
    -> Error {
  return SetPixels(context, imageData.GetSpan(), mipLevel, arrayLayer,
                   imageData.GetDimensions(), {0, 0, 0}, {0, 0, 0},
                   imageData.GetDimensions());
}

auto Texture::MarkUse() -> void {
  lastUsedTimestamp = Graphics::SemaphoreManager::GetSemaphoreValue();
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

auto Texture::GetDefault(const GraphicsContext &context, VkFormat format,
                         Graphics::TextureType textureType)
    -> Result<Ref<Graphics::Texture>> {

  auto key = std::make_pair(format, textureType);
  auto textureIterator = DefaultTextureCache.find(key);
  if (textureIterator != DefaultTextureCache.end()) {
    return textureIterator->second;
  }

  TextureCreationInfo texInfo = {};
  texInfo.size = VkExtent3D{1, 1, 1};
  texInfo.format = format;
  texInfo.usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                  static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
                  static_cast<uint32_t>(VK_IMAGE_USAGE_STORAGE_BIT);

  std::string_view textureTypeName;

  switch (textureType) {
  case TextureType::DEFAULT:
    textureTypeName = "2D";
    break;
  case TextureType::CUBEMAP:
    textureTypeName = "Cubemap";
    break;
  case TextureType::VOLUME:
    textureTypeName = "Volume";
    break;
  case TextureType::ARRAY:
    textureTypeName = "Array";
    break;
  default:
    return Error::Unexpected("Unsupported texture type for default texture");
  }

  texInfo.debugName = std::format("Default_Texture_{}_{}", textureTypeName,
                                  Format::ImageFormatToString(format));

  if (textureType == TextureType::CUBEMAP) {
    texInfo.arrayLayers = 6; // NOLINT
  }
  texInfo.mipmapCount = 1;

  auto texture = CHECK_RES(Create(context, texInfo));

  auto imageDataResult = Image::ImageData::Create(texture->size, format);

  if (Error::IsError(imageDataResult)) {
    return imageDataResult.error();
  }

  auto imageData = imageDataResult.value();
  auto error = imageData->SetColor(
      Math::Uvec3{}, Color(UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX));

  if (Error::IsError(error)) {
    return error;
  }

  auto setPixelsResult = texture->SetPixels(context, *imageData);

  if (Error::IsError(setPixelsResult)) {
    return setPixelsResult;
  }

  DefaultTextureCache[key] = texture;

  return texture;
}

inline auto GetAccessFlagsForUsage(
    TextureUsage usage, VkFormat format,
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE)
    -> VkAccessFlags2 {
  switch (usage) {
  case TextureUsage::Sampler:
    return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  case TextureUsage::Storage:
    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  case TextureUsage::Attachment: {
    bool isDepthStencil = Image::IsDepthOrStencilTexture(format);
    VkAccessFlagBits2 accessFlags = VK_ACCESS_2_NONE;

    if (isDepthStencil) {
      if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
        accessFlags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      }

      if (storeOp == VK_ATTACHMENT_STORE_OP_STORE) {
        accessFlags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      }
    } else {
      if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
        accessFlags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
      }

      if (storeOp == VK_ATTACHMENT_STORE_OP_STORE) {
        accessFlags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      }
    }

    return accessFlags;
  }
  case TextureUsage::TransferSrc:
    return VK_ACCESS_2_TRANSFER_READ_BIT;
  case TextureUsage::TransferDst:
    return VK_ACCESS_2_TRANSFER_WRITE_BIT;
  case TextureUsage::PresentSrc:
  case TextureUsage::Unknown:
  case TextureUsage::Swapchain:
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
  assert(usage != TextureUsage::Unknown &&
         "GetRequiredTextureLayout: TextureUsage::Unknown is not a valid "
         "usage.");
#endif

  switch (usage) {
  case TextureUsage::Sampler:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case TextureUsage::Storage:
    return VK_IMAGE_LAYOUT_GENERAL;
  case TextureUsage::Attachment:
    if (Image::IsDepthOrStencilTexture(format)) {
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else {
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  case TextureUsage::TransferSrc:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case TextureUsage::TransferDst:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case TextureUsage::Unknown:
  case TextureUsage::Swapchain:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case TextureUsage::PresentSrc:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  default:
    PrintError("GetRequiredTextureLayout: Unknown texture usage: {}",
               static_cast<int>(usage));
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

auto Texture::UseAs(const GraphicsContext &context, TextureUsage newUsage,
                    VkPipelineStageFlags2 stage, VkAttachmentLoadOp loadOp,
                    VkAttachmentStoreOp storeOp) -> Error {

  if (newUsage == TextureUsage::Unknown ||
      newUsage == TextureUsage::Swapchain) {
    return Error::Create("UseAs: Unknown or Swapchain is not a valid usage.");
  }

  if (stage == VK_PIPELINE_STAGE_2_NONE) {
    return Error::Create(
        "UseAs: stage must be known when transitioning layouts.");
  }

  auto layout = GetRequiredTextureLayout(newUsage, format);

  VkAccessFlags2 currentAccess = GetAccessFlagsForUsage(lastUsage, format);
  VkAccessFlags2 newAccess = GetAccessFlagsForUsage(newUsage, format);

  if (lastUsage == TextureUsage::Unknown) {
    // First time usage, so we can skip the transition from UNDEFINED
    currentAccess = 0;
    lastPipelineStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  } else if (lastUsage == TextureUsage::Swapchain) {
    currentAccess = VK_ACCESS_2_NONE;
    lastPipelineStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  }

  auto range = VkImageSubresourceRange{
      .aspectMask = GetAspectFlagsForFormat(format),
      .baseMipLevel = 0,
      .levelCount = static_cast<uint32_t>(mipmapcount),
      .baseArrayLayer = 0,
      .layerCount = static_cast<uint32_t>(arrayLayers),
  };

  auto result = TransitionLayout(context, layout, lastPipelineStage, stage,
                                 currentAccess, newAccess, range);

  lastUsage = newUsage;
  lastPipelineStage = stage;
  return result;
}

auto Texture::UseAsAttachment(const GraphicsContext &context,
                              VkAttachmentLoadOp loadOp,
                              VkAttachmentStoreOp storeOp) -> Error {
  // Depth/stencil attachments require both early and late fragment test stages
  if (Image::IsDepthOrStencilTexture(format)) {
    auto newPipelineStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    return UseAs(context, TextureUsage::Attachment, newPipelineStage, loadOp,
                 storeOp);
  }

  auto newPipelineStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  return UseAs(context, TextureUsage::Attachment, newPipelineStage, loadOp,
               storeOp);
}

auto Texture::UseAsSampler(const GraphicsContext &context,
                           VkPipelineStageFlags2 stage) -> Error {
  return UseAs(context, TextureUsage::Sampler, stage);
}
auto Texture::UseAsTransferSrc(const GraphicsContext &context) -> Error {
  return UseAs(context, TextureUsage::TransferSrc,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
}
auto Texture::UseAsTransferDst(const GraphicsContext &context) -> Error {
  return UseAs(context, TextureUsage::TransferDst,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
}
auto Texture::UseAsStorage(const GraphicsContext &context,
                           VkPipelineStageFlags2 stage) -> Error {
  return UseAs(context, TextureUsage::Storage, stage);
}
auto Texture::UseAsPresentSrc(const GraphicsContext &context) -> Error {
  return UseAs(context, TextureUsage::PresentSrc,
               VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
}

auto Texture::CopyTo(const GraphicsContext &context, Texture &dstTexture,
                     CopyRegion region) -> Error {
  if (format != dstTexture.format) {
    return Error::Create(
        "CopyTo: Source and destination textures must have the "
        "same format for copying.");
  }
  if (region.extent.width == 0 || region.extent.height == 0 ||
      region.extent.depth == 0) {
    return Error::Create("CopyTo: Copy extent must be greater than zero.");
  }
  if (region.srcBaseMipLevel >= mipmapcount ||
      region.dstBaseMipLevel >= dstTexture.mipmapcount) {
    return Error::Create(
        "CopyTo: Source and destination mip levels must be within bounds.");
  }

  if (region.srcBaseArrayLayer >= arrayLayers ||
      region.dstBaseArrayLayer >= dstTexture.arrayLayers) {
    return Error::Create(
        "CopyTo: Source and destination array layers must be within bounds.");
  }

  if (region.srcOffset.x < 0 || region.srcOffset.y < 0 ||
      region.srcOffset.z < 0 || region.dstOffset.x < 0 ||
      region.dstOffset.y < 0 || region.dstOffset.z < 0) {
    return Error::Create(
        "CopyTo: Source and destination offsets cannot be negative.");
  }

  if (region.srcOffset.x + region.extent.width > size.width ||
      region.srcOffset.y + region.extent.height > size.height ||
      region.srcOffset.z + region.extent.depth > size.depth) {
    return Error::Create(
        "CopyTo: Source offset and extent exceed source texture dimensions.");
  }

  if (region.dstOffset.x + region.extent.width > dstTexture.size.width ||
      region.dstOffset.y + region.extent.height > dstTexture.size.height ||
      region.dstOffset.z + region.extent.depth > dstTexture.size.depth) {
    return Error::Create(
        "CopyTo: Destination offset and extent exceed destination texture "
        "dimensions.");
  }

  auto *commandBuffer = GetCommandBuffer();
  if (commandBuffer == nullptr) {
    return Error::Create("CopyTo: Failed to get command buffer for copying.");
  }

  CHECK_ERR(UseAsTransferSrc(context));
  CHECK_ERR(dstTexture.UseAsTransferDst(context));

  VkImageCopy copyRegion = {};
  copyRegion.srcSubresource.aspectMask = GetAspectFlagsForFormat(format);
  copyRegion.srcSubresource.mipLevel = region.srcBaseMipLevel;
  copyRegion.srcSubresource.baseArrayLayer = region.srcBaseArrayLayer;
  copyRegion.srcSubresource.layerCount = region.layerCount;
  copyRegion.srcOffset = region.srcOffset;
  copyRegion.dstSubresource.aspectMask =
      GetAspectFlagsForFormat(dstTexture.format);
  copyRegion.dstSubresource.mipLevel = region.dstBaseMipLevel;
  copyRegion.dstSubresource.baseArrayLayer = region.dstBaseArrayLayer;
  copyRegion.dstSubresource.layerCount = region.layerCount;
  copyRegion.dstOffset = region.dstOffset;
  copyRegion.extent = region.extent;

  // TODO: Check if this is needed after UseAsTransferSrc and UseAsTransferDst
  // Barrier::UpdateUsage(context, *this,
  //                      Barrier::ResourceState{
  //                          .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
  //                          .access = VK_ACCESS_2_TRANSFER_READ_BIT,
  //                      });
  // Barrier::UpdateUsage(context, dstTexture,
  //                      Barrier::ResourceState{
  //                          .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
  //                          .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
  //                      });

  vkCmdCopyImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                 &copyRegion);

  MarkUse();
  dstTexture.MarkUse();

  return Error::Success();
}

auto Texture::CopyTo(const GraphicsContext &context, Buffer &dstBuffer,
                     ToBufferCopyRegion region) -> Error {
  VkBufferImageCopy copyRegion = {};
  copyRegion.bufferOffset = region.dstOffset;
  copyRegion.bufferRowLength = region.extent.width;
  copyRegion.bufferImageHeight = region.extent.height;
  copyRegion.imageSubresource.aspectMask = GetAspectFlagsForFormat(format);
  copyRegion.imageSubresource.mipLevel = region.srcBaseMipLevel;
  copyRegion.imageSubresource.baseArrayLayer = region.srcBaseArrayLayer;
  copyRegion.imageSubresource.layerCount = region.layerCount;
  copyRegion.imageOffset = region.srcOffset;
  copyRegion.imageExtent = region.extent;

  auto *commandBuffer = GetCommandBuffer();
  if (commandBuffer == nullptr) {
    return Error::Create("CopyTo: Failed to get command buffer for copying.");
  }

  CHECK_ERR(UseAsTransferSrc(context));

  if ((dstBuffer.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
    return Error::Create("CopyTo: Destination buffer must have "
                         "VK_BUFFER_USAGE_TRANSFER_DST_BIT "
                         "usage flag.");
  }

  Barrier::UpdateUsage(context, dstBuffer,
                       Barrier::ResourceState{
                           .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       });
  vkCmdCopyImageToBuffer(commandBuffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuffer.handle,
                         1, &copyRegion);
  dstBuffer.MarkUse();
  MarkUse();
  return Error::Success();
}

Texture::~Texture() {
  if (isSwapchainView) { // Not owned, don't destroy
    return;
  }

  if (isView) { // Not owned, don't destroy
    parentTexture->release();

    ScheduleDestruction(TextureViewMemory{.imageView = view,
                                          .timelineValue = lastUsedTimestamp});

    return;
  }

  ScheduleDestruction(TextureMemory{.allocation = memory,
                                    .image = image,
                                    .imageView = view,
                                    .timelineValue = lastUsedTimestamp});

  Texture::TotalAllocatedMemory -= sizeInBytes;
}

std::atomic<VkDeviceSize> Texture::TotalAllocatedMemory{};

auto Texture::GenerateMipmaps(GraphicsContext &context) -> Error {
  if (mipmapcount <= 1) {
    return Error::Create("Texture does not have multiple mip levels for "
                         "mipmap generation.");
  }

  auto *commandBuffer = GetCommandBuffer();
  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for mipmap generation.");
  }

  if (Image::IsCompressedTexture(format)) {
    return Error::Create("Automatic mipmap generation is not supported for "
                         "compressed texture formats.");
  }

  DynamicRendering::EndRendering(context);

  auto mipWidth = static_cast<int32_t>(size.width);
  auto mipHeight = static_cast<int32_t>(size.height);

  VkImageMemoryBarrier2 barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcAccessMask = GetAccessFlagsForUsage(lastUsage, format);
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = GetAspectFlagsForFormat(format);
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipmapcount;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = static_cast<uint32_t>(arrayLayers);

  VkDependencyInfo dep = {};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &barrier;

  barrier.srcStageMask = lastPipelineStage;
  barrier.srcAccessMask = GetAccessFlagsForUsage(lastUsage, format);
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  barrier.oldLayout = GetRequiredTextureLayout(lastUsage, format);
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  vkCmdPipelineBarrier2(commandBuffer, &dep);

  barrier.subresourceRange.levelCount = 1;

  // [dst, dst, dst, ...]

  for (uint32_t i = 1; i < mipmapcount; ++i) {
    auto baseExtent = Image::GetDimensions(size, i - 1);
    auto mipExtent = Image::GetDimensions(size, i);

    // mip - 1 is transfer write
    // Now needs transfer read
    // current mip is original layout but needs transfer write

    // We convert mip - 1 to transfer read
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.subresourceRange.baseMipLevel = i - 1;
    // [src, dst, dst, dst, ...]

    vkCmdPipelineBarrier2(commandBuffer, &dep);

    VkImageBlit blit = {};
    blit.srcSubresource.aspectMask = GetAspectFlagsForFormat(format);
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = arrayLayers;
    blit.srcOffsets[0] = {.x = 0, .y = 0, .z = 0};
    blit.srcOffsets[1] = {.x = static_cast<int32_t>(baseExtent.width),
                          .y = static_cast<int32_t>(baseExtent.height),
                          .z = static_cast<int32_t>(baseExtent.depth)};
    blit.dstSubresource.aspectMask = GetAspectFlagsForFormat(format);
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = arrayLayers;
    blit.dstOffsets[0] = {.x = 0, .y = 0, .z = 0};
    blit.dstOffsets[1] = {.x = static_cast<int32_t>(mipExtent.width),
                          .y = static_cast<int32_t>(mipExtent.height),
                          .z = static_cast<int32_t>(mipExtent.depth)};

    vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);
  }

  // The entire mip chain is now in transfer src, except for mip count - 1, which is in transfer dst
  // We need convert the final mip from transfer dst to transfer src, so they all match
  // Then we can signal the entire texture is transfer src, and the next usage will handle the transition from there

  barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.subresourceRange.baseMipLevel = mipmapcount - 1;

  vkCmdPipelineBarrier2(commandBuffer, &dep);

  lastUsage = TextureUsage::TransferSrc;
  lastPipelineStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

  return {};
}

} // namespace Graphics
