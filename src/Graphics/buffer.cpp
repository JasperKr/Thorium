#include "buffer.hpp"
#include "Modules/error.hpp"
#include "graphics.hpp"
#include "vulkan/vulkan_core.h"
#include <cstddef>
#include <span>

auto Graphics::Buffer::Create(Graphics::GraphicsContext &context,
                              Graphics::BufferCreationInfo info)
    -> tl::expected<Graphics::Buffer, Error::Error> {
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = info.size;
  bufferInfo.usage = info.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  Graphics::Buffer outBuffer = {};

  Error::Error error = Error::FromVkResult(
      vkCreateBuffer(context.device, &bufferInfo, nullptr, &outBuffer.Handle));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(context.device, outBuffer.Handle,
                                &memRequirements);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;

  // Pick memory type that satisfies properties
  VkPhysicalDeviceMemoryProperties memProps;
  vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &memProps);

  uint32_t memoryTypeIndex = 0;
  bool found = false;
  auto memoryTypes =
      std::span(&memProps.memoryTypes[0], memProps.memoryTypeCount);

  for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
    if (((memRequirements.memoryTypeBits & (1U << i)) != 0U) &&
        (memoryTypes[i].propertyFlags & info.properties) == info.properties) {
      memoryTypeIndex = i;
      found = true;
      break;
    }
  }

  if (!found) {
    return tl::unexpected(
        Error::Create("Failed to find suitable memory type for buffer."));
  }

  allocInfo.memoryTypeIndex = memoryTypeIndex;

  error = Error::FromVkResult(
      vkAllocateMemory(context.device, &allocInfo, nullptr, &outBuffer.Memory));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  error = Error::FromVkResult(vkBindBufferMemory(
      context.device, outBuffer.Handle, outBuffer.Memory, 0));

  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }
  outBuffer.Size = info.size;

  return outBuffer;
}

auto Graphics::Buffer::SetData(Graphics::GraphicsContext &context,
                               const void *data, VkDeviceSize size,
                               VkDeviceSize offset) const -> Error::Error {
  void *mappedData = nullptr;
  Error::Error error = Error::FromVkResult(
      vkMapMemory(context.device, Memory, offset, size, 0, &mappedData));

  memcpy(mappedData, data, (size_t)size);

  vkUnmapMemory(context.device, Memory);

  return Error::Success();
}

void Graphics::Buffer::Destroy(Graphics::GraphicsContext &context) const {
  vkDestroyBuffer(context.device, Handle, nullptr);
  vkFreeMemory(context.device, Memory, nullptr);
}