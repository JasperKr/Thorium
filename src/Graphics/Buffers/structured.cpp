#include "structured.hpp"

#include <cstdint>

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include "tl/expected.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics {

auto CreateStructuredBuffer(GraphicsContext &context, size_t elementCount,
                            const BufferInfo &layout)
    -> tl::expected<StructuredBuffer, Error::Error> {

  VkMemoryPropertyFlags memoryFlags =
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  // If the buffer is writable, we may want readback as well
  if (layout.access == SLANG_RESOURCE_ACCESS_READ_WRITE ||
      layout.access == SLANG_RESOURCE_ACCESS_WRITE) {
    usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }

  // If the buffer is readable, we may want to upload data to it
  if (layout.access == SLANG_RESOURCE_ACCESS_READ_WRITE ||
      layout.access == SLANG_RESOURCE_ACCESS_READ) {
    usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }

  Graphics::BufferCreationInfo bufferCreateInfo{
      .size = layout.size * elementCount,
      .usage = usageFlags,
      .properties = memoryFlags,
  };

  auto result = Buffer::Create(context, bufferCreateInfo);

  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
  }

  StructuredBuffer structuredBuffer = {
      .elementCount = elementCount,
      .elementStride = layout.size,
      .layout = layout,
      .buffer = result.value(),
  };

  return structuredBuffer;
}

} // namespace Graphics