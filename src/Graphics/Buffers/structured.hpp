#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include "Modules/type.hpp"

namespace Graphics::StructuredBuffer {

const Type type = Type("Buffer");

struct StructuredBuffer : Object {
  [[nodiscard]] auto GetBuffer() const -> Ref<Buffer> { return buffer; }
  [[nodiscard]] auto GetElementCount() const -> size_t { return elementCount; }
  [[nodiscard]] auto GetElementStride() const -> size_t {
    return elementStride;
  }
  [[nodiscard]] auto GetFormat() const -> BufferFormat const & {
    return format;
  }

  // NOLINTNEXTLINE
  auto Clear(GraphicsContext &context, uint32_t value, VkDeviceSize offset = 0,
             VkDeviceSize size = VK_WHOLE_SIZE) -> Error {
    return buffer->Clear(context, value, offset, size);
  }

  auto IsCompatible(BufferInfo &layout) const -> Error {
    for (const auto &component : format.GetComponents()) {
      const auto *field =
          layout.ResolvePath(component.name.begin(), component.name.end());
      if (field == nullptr) {
        return Error(
            "StructuredBuffer: Incompatible buffer layout (missing component " +
            ResourceKeyToString(component.name) + ").");
      }

      if (field->GetOffset() != component.offset) {
        return Error(
            "StructuredBuffer: Incompatible buffer layout (component " +
            ResourceKeyToString(component.name) +
            " has incorrect offset, expected: " +
            std::to_string(component.offset) + ").");
      }

      if (field->GetSize() != Graphics::Format::GetSize(component.format)) {
        return Error(
            "StructuredBuffer: Incompatible buffer layout (component " +
            ResourceKeyToString(component.name) +
            " has incorrect size, expected: " +
            std::to_string(Graphics::Format::GetSize(component.format)) + ").");
      }
    }

    if (layout.size != elementStride) {
      return Error(
          "StructuredBuffer: Incompatible buffer layout size; expected " +
          std::to_string(elementStride) + ", got " +
          std::to_string(layout.size) + ".");
    }

    return Error::Success();
  }

  [[nodiscard]] auto UseDeferredDestruction() const -> bool override {
    return false;
  }

  auto ScheduleDestroy() -> void override { buffer.reset(); }

  static auto GetType() -> Type const * { return &type; }
  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return StructuredBuffer::GetType();
  }

  [[nodiscard]] auto GetSize() const -> size_t {
    return elementCount * elementStride;
  }

  [[nodiscard]] auto GetStride() const -> size_t { return elementStride; }

  BufferFormat format;
  size_t elementCount;
  size_t elementStride;
  Ref<Buffer> buffer;
};

struct StructuredBufferCreationInfo {
  VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkBufferUsageFlags usageFlags =
      static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
      static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
      static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  std::string debugName;
};

auto CreateStructuredBuffer(GraphicsContext &context, BufferFormat &format,
                            size_t elementCount,
                            StructuredBufferCreationInfo const &info)
    -> Result<Ref<StructuredBuffer>>;

} // namespace Graphics::StructuredBuffer