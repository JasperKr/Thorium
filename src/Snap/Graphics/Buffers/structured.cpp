#include "structured.hpp"

#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "tl/expected.hpp"

namespace Graphics {

auto StructuredBuffer::Create(GraphicsContext &context, BufferFormat &format,
                              size_t elementCount,
                              StructuredBufferCreationInfo const &info)
    -> Result<Ref<StructuredBuffer>> {

  if (format.GetStride() == 0) {
    return Error::Unexpectedf(
        "Invalid format, Cannot create structured buffer with a stride of 0");
  }

  Graphics::BufferCreationInfo bufferCreateInfo{
      .size = format.GetStride() * elementCount,
      .usage = info.usageFlags,
      .properties = info.memoryFlags,
      .debugName = info.debugName,
  };

  auto result = Buffer::Create(context, bufferCreateInfo);

  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }

  auto buffer = Ref<StructuredBuffer>::Make();

  buffer->format = format;
  buffer->elementCount = elementCount;
  buffer->elementStride = format.GetStride();
  buffer->buffer = result.value();

  return buffer;
}

auto StructuredBuffer::GetElementStride() const -> size_t {
  return elementStride;
}

auto StructuredBuffer::GetFormat() -> BufferFormat & { return format; }

auto StructuredBuffer::Clear(GraphicsContext &context, uint32_t value,
                             VkDeviceSize offset, VkDeviceSize size) const
    -> Error {
  return buffer->Clear(context, value, offset, size);
}

} // namespace Graphics