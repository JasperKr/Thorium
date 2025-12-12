#include "buffer.hpp"
#include "Graphics/released.hpp"
#include "Graphics/resource.hpp"
#include "Modules/error.hpp"
#include "graphics.hpp"
#include <cstdint>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics {

constexpr size_t UploadBuferSize = 16L * 1024L * 1024L;     // MB
constexpr size_t LargeUploadThreshold = 1L * 1024L * 1024L; // MB

struct StagingBufferInfo {
  VkBuffer buffer;
  VmaAllocation memory;
  uint64_t timelineValue;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

inline std::array<Ref<Buffer>, FRAMES_IN_FLIGHT> UploadBuffers;
inline std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> UploadCommandBuffers;

// we'll use staging buffers until upload buffers are initialized
inline bool UploadBuffersInitialized = false;
// To be deleted
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

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = GetRenderData(context, 0).pool;
  allocInfo.commandBufferCount = FRAMES_IN_FLIGHT;

  vkAllocateCommandBuffers(context.device, &allocInfo,
                           UploadCommandBuffers.data());

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  moduleInitialized = true;

  return Error::Success();
}

auto FlushBufferUploads(GraphicsContext &context) -> Error::Error {
  // Submit upload command buffers
  auto *commandBuffer = UploadCommandBuffers.at(context.frameIndex);

  VkTimelineSemaphoreSubmitInfo timelineSubmitInfo = {};
  timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  uint64_t signalValue = currentTimelineValue;
  timelineSubmitInfo.signalSemaphoreValueCount = 1;
  timelineSubmitInfo.pSignalSemaphoreValues = &signalValue;

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  submitInfo.pNext = &timelineSubmitInfo;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &uploadTimeline;

  auto result = Error::Create(
      vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
  if (Error::IsError(result)) {
    return result;
  }
  result = Error::Create(vkResetCommandBuffer(commandBuffer, 0));
  if (Error::IsError(result)) {
    return result;
  }

  // Check staging buffers for completed uploads
  uint64_t completedValue = 0;
  result = Error::Create(vkGetSemaphoreCounterValue(
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

  return Error::Success();
}

inline auto Upload(Buffer *buffer, GraphicsContext &context,
                   std::span<const uint8_t> data, VkDeviceSize offset = 0)
    -> Error::Error {
  if (!UploadBuffersInitialized || data.size() > LargeUploadThreshold) {
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
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
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

    // Copy from staging buffer to destination buffer
    VkCommandBuffer commandBuffer = UploadCommandBuffers.at(context.frameIndex);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

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

    vkEndCommandBuffer(commandBuffer);

  } else {
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
                     VkDeviceSize offset = 0) const -> Error::Error {
  auto dataSize = data.size();

  void *mapped = nullptr;
  auto result =
      Error::Create(vmaMapMemory(context.vmaAllocator, memory, &mapped));
  if (Error::IsError(result)) {
    return result;
  }

  if (dataSize == 0) {
    dataSize = size;
  }

  if (offset + dataSize > size) {
    vmaUnmapMemory(context.vmaAllocator, memory);
    return Error::Create("Data out of bounds for buffer set data.");
  }

  // NOLINTNEXTLINE, because of pointer arithmetic
  std::memcpy(static_cast<uint8_t *>(mapped) + offset, data.data(),
              dataSize - offset);
  vmaUnmapMemory(context.vmaAllocator, memory);

  return Error::Success();
}

auto Buffer::Release() -> Error::Error {
  if (released) {
    return Error::Create("Buffer already released.");
  }

  released = true;

  ReleasedResource resource;
  resource.type = ReleasedResourceType::BUFFER;
  resource.resource = this;

  AddReleasedResource(resource);

  return Error::Success();
}

auto Buffer::Destroy(GraphicsContext &context) const -> void {
  vmaDestroyBuffer(context.vmaAllocator, handle, memory);
}

auto Buffer::Resize(GraphicsContext &context, VkDeviceSize newSize)
    -> Error::Error {
  // Destroy existing buffer
  vmaDestroyBuffer(context.vmaAllocator, handle, memory);
  // Create new buffer with new size
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = newSize;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide
  allocInfo.requiredFlags = properties;

  VkResult result = vmaCreateBuffer(context.vmaAllocator, &bufferInfo,
                                    &allocInfo, &handle, &memory, nullptr);
  if (result != VK_SUCCESS) {
    return Error::Create(result);
  }

  size = newSize;

  return Error::Success();
}
} // namespace Graphics