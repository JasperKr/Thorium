#include "uniform.hpp"
#include "Graphics/graphics.hpp"
#include <cassert>
#include <public/tracy/Tracy.hpp>
#include <vector>

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics {

thread_local std::vector<FrameUniformBufferObject>
    ThreadUniformBuffers{}; // NOLINT

auto InitializeUniformBufferModule(GraphicsContext &context) -> Error {
  for (uint32_t j = 0; j < FRAMES_IN_FLIGHT; j++) {
    auto createResult = FrameUniformBufferObject::Create(context);

    if (Error::IsError(createResult)) {
      return createResult.error();
    }

    ThreadUniformBuffers.emplace_back(createResult.value());
  }

  return Error::Success();
}

auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void {
  ThreadUniformBuffers.clear();
}

auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject & {
  assert(frameIndex < ThreadUniformBuffers.size() &&
         "Frame index out of bounds in GetGlobalUniformBuffer");
  return ThreadUniformBuffers.at(frameIndex);
}

auto FrameUniformBufferObject::Create(GraphicsContext &context)
    -> Result<FrameUniformBufferObject> {

  FrameUniformBufferObject obj{};
  obj.size = InitialUniformBufferSize;

  BufferCreationInfo info{};
  info.size = obj.size;
  info.persistentMapping = true;
  info.stagingBuffer = true;
  info.properties = static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
                    static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
                    static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
               static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  info.debugName = "Frame Uniform Buffer";

  auto result = Buffer::Create(context, info);
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  obj.buffer = result.value();

  return obj;
}

void FrameUniformBufferObject::SetData(const Graphics::GraphicsContext &context,
                                       const std::span<const uint8_t> &data,
                                       uint32_t atOffset) {
  if (atOffset + data.size() > localData.size()) {
    localData.resize(atOffset + data.size());
  }

  // NOLINTNEXTLINE, pointer arithmetic
  memcpy(localData.data() + atOffset, data.data(), data.size());
}

auto FrameUniformBufferObject::Flush(const Graphics::GraphicsContext &context)
    -> Result<bool> {
  ZoneScoped;

  if (localData.size() > MaximumUniformBufferSize) {
    return Error::Unexpected(
        "Tried to set uniform buffer data larger than maximum. (holy shit)");
  }

  bool resized = false;
  if (localData.size() + offset > size) {
    while (localData.size() + offset > size) {
      size *= 2;
      if (size > MaximumUniformBufferSize) {
        return Error::Unexpected(
            "Uniform buffer exceeded maximum allowed size.");
      }
    }

    Graphics::BufferCreationInfo info{};
    info.size = size;
    info.persistentMapping = true;
    info.stagingBuffer = true;
    info.properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                 static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    info.debugName = "Frame Uniform Buffer";

    auto result = Graphics::Buffer::Create(context, info);
    if (Error::IsError(result)) {
      return result.error().AsUnexpected();
    }

    buffer = result.value();
    resized = true;
  }

  Barrier::UpdateUsage(context, *buffer,
                       Graphics::Barrier::ResourceState{
                           .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                           .access = VK_ACCESS_2_HOST_WRITE_BIT,
                       });

  auto result = buffer->SetData(context, localData, offset);
  auto initialOffset = offset;

  offset += static_cast<uint32_t>(localData.size());

  auto alignment =
      context.deviceProperties.limits.minUniformBufferOffsetAlignment;

  // Align offset to minUniformBufferOffsetAlignment
  offset = (offset + alignment - 1) & ~(alignment - 1);

  lastFlushSize = offset - initialOffset; // Use the difference as flushed size
  localData.clear();

  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  return resized;
}

} // namespace Graphics