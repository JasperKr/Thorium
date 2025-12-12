#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "graphics.hpp"
#include <span>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
namespace Graphics {

struct BufferCreationInfo {
  VkDeviceSize size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags properties;
};

auto FlushBufferUploads(GraphicsContext &context) -> Error::Error;

struct Buffer : Object {
  VkBuffer handle;
  VmaAllocation memory;
  VkDeviceSize size;
  uint64_t sizeInBytes;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags properties;

  static auto Create(Graphics::GraphicsContext &context,
                     Graphics::BufferCreationInfo info)
      -> tl::expected<Ref<Graphics::Buffer>, Error::Error>;

  void Destroy(GraphicsContext &context) const;
  auto SetData(GraphicsContext &context, std::span<const uint8_t> data,
               VkDeviceSize offset) const -> Error::Error;
  template <typename T>
  auto SetData(GraphicsContext &context, std::span<T> data,
               VkDeviceSize offset = 0) const -> Error::Error {
    auto byteSpan = // NOLINTNEXTLINE
        std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.data()),
                                 sizeof(T) * data.size());
    return SetData(context, byteSpan, offset);
  }
  template <typename T>
  auto SetData(GraphicsContext &context, const std::vector<T> &data,
               VkDeviceSize offset = 0) const -> Error::Error {
    auto byteSpan = // NOLINTNEXTLINE
        std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.data()),
                                 sizeof(T) * data.size());
    return SetData(context, byteSpan, offset);
  }

  // Resizes the buffer to the new size. Note: This will destroy the existing
  // buffer and create a new one. Data will be lost. TODO: Implement data copy.
  auto Resize(GraphicsContext &context, VkDeviceSize newSize) -> Error::Error;
};
} // namespace Graphics