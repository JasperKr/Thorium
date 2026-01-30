#include "structured.hpp"

#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "tl/expected.hpp"

namespace Graphics::StructuredBuffer {

auto CreateStructuredBuffer(GraphicsContext &context, BufferFormat &format,
                            size_t elementCount,
                            StructuredBufferCreationInfo const &info)
    -> Result<Ref<StructuredBuffer>> {

  Graphics::BufferCreationInfo bufferCreateInfo{
      .size = format.GetSize() * elementCount,
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
  buffer->elementStride = format.GetSize();
  buffer->buffer = result.value();

  return buffer;
}

} // namespace Graphics::StructuredBuffer