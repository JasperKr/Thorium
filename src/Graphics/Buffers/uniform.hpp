#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "tl/expected.hpp"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <cstdint>

constexpr size_t InitialUniformBufferSize = 64L * 1024; // 64 KB

// 16 MB; allows for doubling in size 8x
constexpr size_t MaximumUniformBufferSize = 64L * 1024 * 1024;

namespace Graphics {

struct FrameUniformBufferObject {
public:
  static auto Create(GraphicsContext &context)
      -> Result<FrameUniformBufferObject> {

    FrameUniformBufferObject obj{};
    obj.size = InitialUniformBufferSize;

    BufferCreationInfo info{};
    info.size = obj.size;
    info.PersistentMapping = true;
    info.IsStagingBuffer = true;
    info.properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                 static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    auto result = Buffer::Create(context, info);
    if (Error::IsError(result)) {
      return result.error().AsUnexpected();
    }

    obj.buffer = result.value();

    return obj;
  }

  void SetData(Graphics::GraphicsContext &context,
               const std::span<const uint8_t> &data, uint32_t atOffset) {
    if (atOffset + data.size() > localData.size()) {
      localData.resize(atOffset + data.size());
    }

    // NOLINTNEXTLINE, pointer arithmetic
    memcpy(localData.data() + atOffset, data.data(), data.size());
  }

  auto Flush(Graphics::GraphicsContext &context) -> Result<bool> {
    if (localData.size() > MaximumUniformBufferSize) {
      return Error::Unexpected(
          "Tried to set uniform buffer data larger than maximum. (holy shit)");
    }

    bool resized = false;
    if (localData.size() + offset > size) {
      if (buffer.get() != nullptr) {
        buffer->ScheduleDestroy();
      }
      while (localData.size() + offset > size) {
        size *= 2;
        if (size > MaximumUniformBufferSize) {
          return Error::Unexpected(
              "Uniform buffer exceeded maximum allowed size.");
        }
      }

      Graphics::BufferCreationInfo info{};
      info.size = size;
      info.PersistentMapping = true;
      info.IsStagingBuffer = true;
      info.properties =
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                   static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

      auto result = Graphics::Buffer::Create(context, info);
      if (Error::IsError(result)) {
        return result.error().AsUnexpected();
      }

      buffer = result.value();
      resized = true;
    }

    auto result = buffer->SetData(context, localData, offset);
    auto initialOffset = offset;

    offset += static_cast<uint32_t>(localData.size());

    auto alignment =
        context.deviceProperties.limits.minUniformBufferOffsetAlignment;

    // Align offset to minUniformBufferOffsetAlignment
    offset = (offset + alignment - 1) & ~(alignment - 1);

    lastFlushSize =
        offset - initialOffset; // Use the difference as flushed size
    localData.clear();

    if (Error::IsError(result)) {
      return result.AsUnexpected();
    }

    return resized;
  }

  auto ScheduleDestroy() -> void {
    if (buffer.get() != nullptr) {
      buffer->ScheduleDestroy();
    }
  }

  auto Destroy(GraphicsContext &context) -> void {
    if (buffer.get() != nullptr) {
      buffer->Destroy(context);
    }
  }

  [[nodiscard]] auto GetOffset() const -> uint32_t { return offset; }
  [[nodiscard]] auto GetSize() const -> uint32_t { return size; }
  [[nodiscard]] auto GetLastFlushSize() const -> uint32_t {
    return lastFlushSize;
  }
  [[nodiscard]] auto GetBuffer() const -> Ref<Graphics::Buffer> {
    return buffer;
  }
  [[nodiscard]] auto NewFrame(Graphics::GraphicsContext &context) -> Error {
    offset = 0;
    return Error::Success();
  }

private:
  Ref<Graphics::Buffer> buffer;
  std::vector<uint8_t> localData;

  uint32_t size{};
  uint32_t lastFlushSize{};
  uint32_t offset{};
};

extern thread_local std::vector<std::vector<FrameUniformBufferObject>>
    ThreadUniformBuffers; // NOLINT
auto InitializeUniformBufferModule(GraphicsContext &context) -> Error;
auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject &;
auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void;

} // namespace Graphics