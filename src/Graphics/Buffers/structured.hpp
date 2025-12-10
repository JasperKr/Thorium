#pragma once

#include <cstddef>

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include "tl/expected.hpp"

namespace Graphics {
struct StructuredBuffer {
  auto GetBuffer() const -> Ref<Buffer> { return buffer; }
  auto GetElementCount() const -> size_t { return elementCount; }
  auto GetElementStride() const -> size_t { return elementStride; }
  auto GetLayout() const -> const BufferInfo & { return layout; }

  size_t elementCount;
  size_t elementStride;
  BufferInfo layout;
  Ref<Buffer> buffer;
};

auto CreateStructuredBuffer(GraphicsContext &context, size_t elementCount,
                            const BufferInfo &layout)
    -> tl::expected<StructuredBuffer, Error::Error>;

} // namespace Graphics