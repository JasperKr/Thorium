#pragma once

#include "Modules/error.hpp"
#include "graphics.hpp"
#include "vulkan/vulkan_core.h"
namespace Graphics {

struct BufferCreationInfo {
  VkDeviceSize size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags properties;
};

struct Buffer {
  VkBuffer Handle;
  VkDeviceMemory Memory;
  VkDeviceSize Size;

  static auto Create(Graphics::GraphicsContext &context,
                     Graphics::BufferCreationInfo info)
      -> tl::expected<Graphics::Buffer, Error::Error>;

  void Destroy(GraphicsContext &context) const;
  auto SetData(GraphicsContext &context, const void *data, VkDeviceSize size,
               VkDeviceSize offset) const -> Error::Error;
};
} // namespace Graphics