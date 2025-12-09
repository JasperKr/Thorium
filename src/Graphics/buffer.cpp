#include "buffer.hpp"
#include "Modules/error.hpp"
#include "graphics.hpp"
#include <cstdint>
#include <iostream>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

auto Graphics::Buffer::Create(Graphics::GraphicsContext &context,
                              Graphics::BufferCreationInfo info)
    -> tl::expected<Ref<Graphics::Buffer>, Error::Error> {

  auto buffer = Ref<Graphics::Buffer>::Make();

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = info.size;
  bufferInfo.usage = info.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide
  allocInfo.requiredFlags = info.properties;
  allocInfo.flags = static_cast<uint32_t>(VMA_ALLOCATION_CREATE_MAPPED_BIT) |
                    static_cast<uint32_t>(
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  VkResult result =
      vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                      &buffer->handle, &buffer->memory, nullptr);

  if (result != VK_SUCCESS) {
    return tl::unexpected(Error::Create(result));
  }

  buffer->size = info.size;
  buffer->usage = info.usage;
  buffer->properties = info.properties;

  VmaAllocationInfo memRequirements;
  vmaGetAllocationInfo(context.vmaAllocator, buffer->memory, &memRequirements);
  buffer->sizeInBytes = memRequirements.size;

  return buffer;
}

auto Graphics::Buffer::SetData(Graphics::GraphicsContext &context,
                               std::span<const uint8_t> data,
                               VkDeviceSize offset = 0) const -> Error::Error {
  auto dataSize = data.size();

  std::cout << "Setting buffer data of size " << dataSize << " bytes at offset "
            << offset << "\n";
  void *mapped = nullptr;
  auto result =
      Error::Create(vmaMapMemory(context.vmaAllocator, memory, &mapped));
  if (Error::IsError(result)) {
    return result;
  }

  if (dataSize == 0) {
    dataSize = size;
  }

  if (offset + dataSize > size) {
    vmaUnmapMemory(context.vmaAllocator, memory);
    return Error::Create("Data out of bounds for buffer set data.");
  }

  // NOLINTNEXTLINE, because of pointer arithmetic
  std::memcpy(static_cast<uint8_t *>(mapped) + offset, data.data(),
              dataSize - offset);
  vmaUnmapMemory(context.vmaAllocator, memory);

  return Error::Success();
}

void Graphics::Buffer::Destroy(Graphics::GraphicsContext &context) const {
  vmaDestroyBuffer(context.vmaAllocator, handle, memory);
}

auto Graphics::Buffer::Resize(Graphics::GraphicsContext &context,
                              VkDeviceSize newSize) -> Error::Error {
  // Destroy existing buffer
  vmaDestroyBuffer(context.vmaAllocator, handle, memory);
  // Create new buffer with new size
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = newSize;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide
  allocInfo.requiredFlags = properties;

  VkResult result = vmaCreateBuffer(context.vmaAllocator, &bufferInfo,
                                    &allocInfo, &handle, &memory, nullptr);
  if (result != VK_SUCCESS) {
    return Error::Create(result);
  }

  size = newSize;

  return Error::Success();
}