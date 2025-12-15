#include "buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/resource.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "graphics.hpp"
#include <cstdint>
#define VK_NO_PROTOTYPES
#include "array"
#include "vulkan/vulkan_core.h"

namespace Graphics {

constexpr size_t UploadBufferSize = static_cast<size_t>(16L * 1024L * 1024L);
constexpr size_t LargeUploadThreshold = static_cast<size_t>(128L * 1024L);

struct StagingBufferInfo {
  VkBuffer buffer;
  VmaAllocation memory;
  uint64_t timelineValue;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

inline std::array<Ref<Buffer>, FRAMES_IN_FLIGHT> UploadBuffers;
inline std::array<size_t, FRAMES_IN_FLIGHT> UploadBufferOffsets;

// we'll use staging buffers until upload buffers are initialized
inline std::vector<StagingBufferInfo> StagingBuffers;
inline VkSemaphore uploadTimeline = nullptr;
inline bool moduleInitialized = false;
inline uint64_t currentTimelineValue = 0;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto LoadBufferModule(GraphicsContext &context) -> Error::Error {
  VkSemaphoreTypeCreateInfo timelineInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
  };

  VkSemaphoreCreateInfo semInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineInfo,
  };

  auto result = Error::Create(
      vkCreateSemaphore(context.device, &semInfo, nullptr, &uploadTimeline));
  if (Error::IsError(result)) {
    return result;
  }

  if (Error::IsError(result)) {
    return result;
  }

  moduleInitialized = true;

  return Error::Success();
}

auto UnloadBufferModule(GraphicsContext &context) -> Error::Error {
  for (auto &stagingBuffer : StagingBuffers) {
    vmaDestroyBuffer(context.vmaAllocator, stagingBuffer.buffer,
                     stagingBuffer.memory);
  }
  StagingBuffers.clear();

  for (auto &uploadBuffer : UploadBuffers) {
    if (uploadBuffer.get() != nullptr) {
      uploadBuffer->ScheduleDestroy();
    }
  }

  if (uploadTimeline != nullptr) {
    vkDestroySemaphore(context.device, uploadTimeline, nullptr);
    uploadTimeline = nullptr;
  }

  moduleInitialized = false;

  return Error::Success();
}

// To be called at the end of each frame that uses uploads
auto FlushBufferUploads(GraphicsContext &context) -> Error::Error {
  // Check staging buffers for completed uploads
  uint64_t completedValue = 0;
  auto result = Error::Create(vkGetSemaphoreCounterValue(
      context.device, uploadTimeline, &completedValue));
  if (Error::IsError(result)) {
    return result;
  }

  auto stagingBufferIterator = StagingBuffers.begin();
  while (stagingBufferIterator != StagingBuffers.end()) {
    if (stagingBufferIterator->timelineValue <= completedValue) {
      // Upload completed, destroy staging buffer
      vmaDestroyBuffer(context.vmaAllocator, stagingBufferIterator->buffer,
                       stagingBufferIterator->memory);
      stagingBufferIterator = StagingBuffers.erase(stagingBufferIterator);
    } else {
      ++stagingBufferIterator;
    }
  }

  // Reset upload buffer offsets for next frame
  for (auto &offset : UploadBufferOffsets) {
    offset = 0;
  }

  return Error::Success();
}

inline auto Upload(const Buffer *buffer, GraphicsContext &context,
                   std::span<const uint8_t> data, VkDeviceSize offset = 0)
    -> Error::Error {

  if (data.size() > LargeUploadThreshold) {
    // Use staging buffer
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = data.size();
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // See: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
    VmaAllocationCreateInfo stagingAllocInfo = {};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags =
        static_cast<uint32_t>(
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) |
        static_cast<uint32_t>(VMA_ALLOCATION_CREATE_MAPPED_BIT);
    VkBuffer stagingBuffer = nullptr;
    VmaAllocation stagingMemory = nullptr;
    VkResult result = vmaCreateBuffer(context.vmaAllocator, &stagingBufferInfo,
                                      &stagingAllocInfo, &stagingBuffer,
                                      &stagingMemory, nullptr);
    if (result != VK_SUCCESS) {
      return Error::Create(result);
    }

    void *mapped = nullptr;
    result = vmaMapMemory(context.vmaAllocator, stagingMemory, &mapped);
    if (result != VK_SUCCESS) {
      vmaDestroyBuffer(context.vmaAllocator, stagingBuffer, stagingMemory);
      return Error::Create(result);
    }

    std::memcpy(static_cast<uint8_t *>(mapped), data.data(), data.size());
    vmaUnmapMemory(context.vmaAllocator, stagingMemory);

    auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = data.size();
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer->handle, 1,
                    &copyRegion);
    StagingBufferInfo stagingInfo = {};
    stagingInfo.buffer = stagingBuffer;
    stagingInfo.memory = stagingMemory;
    stagingInfo.timelineValue = ++currentTimelineValue;

    StagingBuffers.push_back(stagingInfo);
  } else {
    // Use upload buffer
    auto uploadBuffer = UploadBuffers.at(context.frameIndex);
    size_t &uploadOffset = UploadBufferOffsets.at(context.frameIndex);
    if (uploadBuffer.get() == nullptr ||
        uploadBuffer->sizeInBytes < data.size() + uploadOffset) {
      // Create or resize upload buffer
      if (uploadBuffer.get() != nullptr) {
        uploadBuffer->ScheduleDestroy();
      }

      size_t newSize = UploadBufferSize;
      while (newSize < data.size() + uploadOffset) {
        newSize *= 2;
      }

      Graphics::BufferCreationInfo bufferInfo{};
      bufferInfo.size = newSize;
      bufferInfo.usage =
          static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
      bufferInfo.properties =
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
          static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      auto result = Graphics::Buffer::Create(context, bufferInfo);
      if (Error::IsError(result)) {
        return result.error();
      }

      uploadBuffer = result.value();
      UploadBuffers.at(context.frameIndex) = uploadBuffer;
    }

    // Copy data to upload buffer
    void *mapped = nullptr;
    auto result = Error::Create(
        vmaMapMemory(context.vmaAllocator, uploadBuffer->memory, &mapped));
    if (Error::IsError(result)) {
      return result;
    }

    // NOLINTNEXTLINE, because of pointer arithmetic
    std::memcpy(static_cast<uint8_t *>(mapped) + uploadOffset, data.data(),
                data.size());
    vmaUnmapMemory(context.vmaAllocator, uploadBuffer->memory);

    // Record copy command
    auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = uploadOffset;
    copyRegion.dstOffset = offset;
    copyRegion.size = data.size();
    vkCmdCopyBuffer(commandBuffer, uploadBuffer->handle, buffer->handle, 1,
                    &copyRegion);
    uploadOffset += data.size();
  }

  return Error::Success();
}

auto Buffer::Create(GraphicsContext &context, BufferCreationInfo info)
    -> tl::expected<Ref<Buffer>, Error::Error> {
  if (!moduleInitialized) {
    auto result = LoadBufferModule(context);
    if (Error::IsError(result)) {
      return tl::unexpected(result);
    }
  }

  auto buffer = Ref<Buffer>::Make();

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = info.size;
  bufferInfo.usage = info.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide
  allocInfo.requiredFlags = info.properties;
  allocInfo.flags = static_cast<uint32_t>(VMA_ALLOCATION_CREATE_MAPPED_BIT) |
                    static_cast<uint32_t>(
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  VkResult result =
      vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                      &buffer->handle, &buffer->memory, nullptr);

  if (result != VK_SUCCESS) {
    return tl::unexpected(Error::Create(result));
  }

  buffer->size = info.size;
  buffer->usage = info.usage;
  buffer->properties = info.properties;

  VmaAllocationInfo memRequirements;
  vmaGetAllocationInfo(context.vmaAllocator, buffer->memory, &memRequirements);
  buffer->sizeInBytes = memRequirements.size;

  return buffer;
}

auto Buffer::SetData(GraphicsContext &context, std::span<const uint8_t> data,
                     VkDeviceSize offset = 0) -> Error::Error {

  auto dataSize = data.size();

  auto result = Upload(this, context, data, offset);
  if (Error::IsError(result)) {
    return result;
  }

  auto timelineValueResult = IncrementTimelineSemaphore(context);

  if (Error::IsError(timelineValueResult)) {
    return timelineValueResult.error();
  }

  auto timelineValue = timelineValueResult.value();

  MarkUse(0, timelineValue); // TODO: Support multiple queues

  return Error::Success();
}

auto Buffer::Destroy(GraphicsContext &context) const -> void {
  vmaDestroyBuffer(context.vmaAllocator, handle, memory);
}

auto Buffer::ScheduleDestroy() -> bool {
  if (released) {
    return false;
  }

  ReleasedBuffers.emplace_back(this);
  released = true;

  return true;
}

} // namespace Graphics