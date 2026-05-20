#include "uniform.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include <cassert>
#include <cstddef>
#include <public/tracy/Tracy.hpp>
#include <vector>

#include "Modules/Helpers/utils.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics {

thread_local std::vector<FrameUniformBufferObject>
    ThreadUniformBuffers{}; // NOLINT

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<int> UniformBufferObjectCount{0};

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
  BufferCreationInfo info{};
  info.size = InitialUniformBufferSize;
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
    return result.error();
  }
  obj.buffer = result.value();

  return obj;
}

auto FrameUniformBufferObject::Write(const Graphics::GraphicsContext &context,
                                     const std::span<const uint8_t> &data,
                                     size_t writeOffset) -> Result<bool> {
  ZoneScoped;

  if (data.size_bytes() == 0) {
    return false;
  }

  if (data.size_bytes() > MaximumUniformBufferSize) {
    return Error::Unexpected(
        "Tried to set uniform buffer data larger than maximum. (holy shit)");
  }

  bool resized = false;
  size_t bufferSize = buffer->size;
  if (data.size_bytes() + offset > bufferSize) {
    while (data.size_bytes() + offset > bufferSize) {
      bufferSize *= 2;
      if (bufferSize > MaximumUniformBufferSize) {
        return Error::Unexpected(
            "Uniform buffer exceeded maximum allowed size.");
      }
    }

    Graphics::BufferCreationInfo info{};
    info.size = bufferSize;
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
      return result.error();
    }

    buffer = result.value();
    resized = true;
  }

  Barrier::UpdateUsage(context, *buffer,
                       Graphics::Barrier::ResourceState{
                           .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                           .access = VK_ACCESS_2_HOST_WRITE_BIT,
                       });

  auto result = buffer->SetData(context, data, offset + writeOffset);
  auto initialOffset = offset;

  offset += static_cast<uint32_t>(data.size_bytes() + writeOffset);
  offset = Utils::AlignUp(
      offset, context.deviceProperties.limits.minUniformBufferOffsetAlignment);

  if (Error::IsError(result)) {
    return result;
  }

  return resized;
}

} // namespace Graphics