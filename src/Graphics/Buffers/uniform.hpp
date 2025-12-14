#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "tl/expected.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstdint>

constexpr size_t InitialUniformBufferSize = 64L * 1024; // 64 KB

// 16 MB; allows for doubling in size 8x
constexpr size_t MaximumUniformBufferSize = 64L * 1024 * 1024;

namespace Graphics {

struct FrameUniformBufferObject {
public:
  explicit FrameUniformBufferObject(GraphicsContext &context)
      : size(static_cast<uint32_t>(InitialUniformBufferSize)) {

    BufferCreationInfo info{};
    info.size = size;
    info.properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                 static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    auto result = Buffer::Create(context, info);
    if (Error::IsError(result)) {
      PrintError("Failed to create frame uniform buffer object.");
      return;
    }

    buffer = result.value();
  }

  auto SetData(Graphics::GraphicsContext &context,
               const std::span<const uint8_t> &data)
      -> tl::expected<bool, Error::Error> {

    if (data.size() > MaximumUniformBufferSize) {
      return Error::Unexpected(
          "Tried to set uniform buffer data larger than maximum. (holy shit)");
    }

    bool resized = false;
    if (data.size() + offset > size) {
      if (buffer.get() != nullptr) {
        buffer->ScheduleDestroy();
      }
      while (data.size() + offset > size) {
        size *= 2;
        if (size > MaximumUniformBufferSize) {
          return Error::Unexpected(
              "Uniform buffer exceeded maximum allowed size.");
        }
      }

      Graphics::BufferCreationInfo info{};
      info.size = size;
      info.properties =
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                   static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

      auto result = Graphics::Buffer::Create(context, info);
      if (Error::IsError(result)) {
        return tl::make_unexpected(result.error());
      }

      buffer = result.value();
      resized = true;
    }

    auto result = buffer->SetData(context, data, offset);

    offset += static_cast<uint32_t>(data.size());

    if (Error::IsError(result)) {
      return tl::make_unexpected(result);
    }

    return resized;
  }
  [[nodiscard]] auto GetOffset() const -> uint32_t { return offset; }
  [[nodiscard]] auto GetSize() const -> uint32_t { return size; }
  [[nodiscard]] auto GetBuffer() const -> Ref<Graphics::Buffer> {
    return buffer;
  }
  [[nodiscard]] auto NewFrame(Graphics::GraphicsContext &context)
      -> Error::Error {
    offset = 0;
    return Error::Success();
  }

private:
  Ref<Graphics::Buffer> buffer;

  uint32_t size{};
  uint32_t offset{};
};

// NOLINTNEXTLINE
extern thread_local std::vector<FrameUniformBufferObject> ThreadUniformBuffers;
auto InitializeUniformBufferModule(GraphicsContext &context) -> Error::Error;
auto GetGlobalUniformBuffer() -> FrameUniformBufferObject &;

} // namespace Graphics