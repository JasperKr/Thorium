#include "structured.hpp"

#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "tl/expected.hpp"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics::StructuredBuffer {

auto CreateStructuredBuffer(GraphicsContext &context, BufferFormat &format,
                            size_t elementCount,
                            VkMemoryPropertyFlags memoryFlags,
                            VkBufferUsageFlags usageFlags)
    -> tl::expected<Ref<StructuredBuffer>, Error::Error> {

  Graphics::BufferCreationInfo bufferCreateInfo{
      .size = format.GetSize() * elementCount,
      .usage = usageFlags,
      .properties = memoryFlags,
  };

  auto result = Buffer::Create(context, bufferCreateInfo);

  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
  }

  auto buffer = Ref<StructuredBuffer>::Make();

  buffer->format = format;
  buffer->elementCount = elementCount;
  buffer->elementStride = format.GetSize();
  buffer->buffer = result.value();

  return buffer;
}

} // namespace Graphics::StructuredBuffer