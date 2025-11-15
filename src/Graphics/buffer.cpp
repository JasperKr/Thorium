#include "buffer.hpp"
#include "Modules/error.hpp"
#include "graphics.hpp"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

auto Graphics::Buffer::Create(Graphics::GraphicsContext &context,
                              Graphics::BufferCreationInfo info)
    -> tl::expected<Graphics::Buffer, Error::Error> {

  Graphics::Buffer buffer = {};

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = info.size;
  bufferInfo.usage = info.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide
  allocInfo.requiredFlags = info.properties;

  VkResult result =
      vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                      &buffer.handle, &buffer.memory, nullptr);

  if (result != VK_SUCCESS) {
    return tl::unexpected(Error::FromVkResult(result));
  }

  buffer.size = info.size;
  buffer.usage = info.usage;
  buffer.properties = info.properties;

  VmaAllocationInfo memRequirements;
  vmaGetAllocationInfo(context.vmaAllocator, buffer.memory, &memRequirements);
  buffer.sizeInBytes = memRequirements.size;

  return buffer;
}

auto Graphics::Buffer::SetData(Graphics::GraphicsContext &context,
                               const void *srcData,
                               VkDeviceSize size = 0, // NOLINT
                               VkDeviceSize offset = 0) const -> Error::Error {
  void *mapped = nullptr;
  auto result =
      Error::FromVkResult(vmaMapMemory(context.vmaAllocator, memory, &mapped));
  if (Error::IsError(result)) {
    return result;
  }

  if (size == 0) {
    size = size;
  }

  if (offset + size > size) {
    vmaUnmapMemory(context.vmaAllocator, memory);
    return Error::Create("Data out of bounds for buffer set data.");
  }

  // NOLINTNEXTLINE, because of pointer arithmetic
  std::memcpy(static_cast<uint8_t *>(mapped) + offset, srcData, size - offset);
  vmaUnmapMemory(context.vmaAllocator, memory);

  return Error::Success();
}

void Graphics::Buffer::Destroy(Graphics::GraphicsContext &context) const {
  vmaDestroyBuffer(context.vmaAllocator, handle, memory);
}