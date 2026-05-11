#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Modules/error.hpp"
#include "Modules/type.hpp"

namespace Graphics {

const Type LuaBufferType = Type("Buffer");

struct StructuredBufferCreationInfo {
  VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkBufferUsageFlags usageFlags =
      static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
      static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
      static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  std::string debugName;
};

struct StructuredBuffer : Object {
  [[nodiscard]] auto GetBuffer() const -> Ref<Buffer> { return buffer; }
  [[nodiscard]] auto GetElementCount() const -> size_t { return elementCount; }
  [[nodiscard]] auto GetFormat() -> BufferFormat &;

  // NOLINTNEXTLINE
  auto Clear(GraphicsContext &context, uint32_t value, VkDeviceSize offset = 0,
             VkDeviceSize size = VK_WHOLE_SIZE) const -> Error;

  [[nodiscard]] auto UseDeferredDestruction() const -> bool override {
    return false;
  }

  auto ScheduleDestroy() -> void override { buffer.reset(); }

  static auto GetType() -> Type const * { return &LuaBufferType; }
  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return StructuredBuffer::GetType();
  }

  [[nodiscard]] auto GetSize() const -> size_t {
    return elementCount * elementStride;
  }

  [[nodiscard]] auto GetStride() const -> size_t { return elementStride; }

  [[nodiscard]] auto GetBuffer() -> Ref<Buffer> { return buffer; }

  static auto Create(GraphicsContext &context, BufferFormat &format,
                     size_t elementCount,
                     StructuredBufferCreationInfo const &info)
      -> Result<Ref<StructuredBuffer>>;

private:
  BufferFormat format;
  size_t elementCount;
  size_t elementStride;
  Ref<Buffer> buffer;
};

} // namespace Graphics