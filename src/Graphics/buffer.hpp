#pragma once

#include "Modules/error.hpp"
#include "graphics.hpp"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
namespace Graphics {

struct BufferCreationInfo {
  VkDeviceSize size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags properties;
};

struct Buffer {
  VkBuffer handle;
  VmaAllocation memory;
  VkDeviceSize size;
  uint64_t sizeInBytes;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags properties;

  static auto Create(Graphics::GraphicsContext &context,
                     Graphics::BufferCreationInfo info)
      -> tl::expected<Graphics::Buffer, Error::Error>;

  void Destroy(GraphicsContext &context) const;
  auto SetData(GraphicsContext &context, const void *data, VkDeviceSize size,
               VkDeviceSize offset) const -> Error::Error;
};
} // namespace Graphics