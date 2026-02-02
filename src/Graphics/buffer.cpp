#include "buffer.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/resource.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "graphics.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <vector>
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

thread_local inline std::vector<Ref<Buffer>> UploadBuffers{FRAMES_IN_FLIGHT};
thread_local inline std::array<size_t, FRAMES_IN_FLIGHT> UploadBufferOffsets;

// we'll use staging buffers until upload buffers are initialized
thread_local inline std::vector<StagingBufferInfo> StagingBuffers;
inline std::mutex uploadSemaphoreMutex{};
inline VkSemaphore uploadSemaphore = nullptr;
inline std::atomic<uint64_t> currentTimelineValue = 0;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto LoadBufferModule(GraphicsContext &context) -> Error {
  VkSemaphoreTypeCreateInfo timelineInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
  };

  VkSemaphoreCreateInfo semInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineInfo,
  };

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    std::lock_guard<std::mutex> lock2(uploadSemaphoreMutex);
    auto result = Error::Create(
        vkCreateSemaphore(context.device, &semInfo, nullptr, &uploadSemaphore));
    if (Error::IsError(result)) {
      return result;
    }
  }

  return Error::Success();
}

auto UnloadLocalBufferModule(GraphicsContext &context) -> Error {
  StagingBuffers.clear();

  UploadBuffers.clear();

  return Error::Success();
}

auto UnloadBufferModule(const GraphicsContext &context) -> Error {
  if (uploadSemaphore != nullptr) {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    std::lock_guard<std::mutex> lock2(uploadSemaphoreMutex);
    vkDestroySemaphore(context.device, uploadSemaphore, nullptr);
    uploadSemaphore = nullptr;
  }

  return Error::Success();
}

// To be called at the end of each frame that uses uploads
auto FlushBufferUploads(GraphicsContext &context) -> Error {
  // Check staging buffers for completed uploads
  uint64_t completedValue = 0;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    std::lock_guard<std::mutex> lock2(uploadSemaphoreMutex);
    auto result = Error::Create(vkGetSemaphoreCounterValue(
        context.device, uploadSemaphore, &completedValue));
    if (Error::IsError(result)) {
      return result;
    }
  }

  auto stagingBufferIterator = StagingBuffers.begin();
  while (stagingBufferIterator != StagingBuffers.end()) {
    if (stagingBufferIterator->timelineValue <= completedValue) {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.vmaAllocator);

      // Upload completed, destroy staging buffer
      if (stagingBufferIterator->memory != VK_NULL_HANDLE) {
        vmaUnmapMemory(context.vmaAllocator, stagingBufferIterator->memory);
      }
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

auto Buffer::MapMemory(GraphicsContext &context) -> Error {
  if (persistentMapping) {
    if (mappedData != nullptr) {
      return Error::Success();
    }

    return Error::Create("Buffer was created with persistent mapping, but "
                         "mappedData is null.");
  }

  std::lock_guard<std::mutex> lock(
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  auto result = Error::Create(
      vmaMapMemory(context.vmaAllocator, this->memory, &mappedData));
  if (Error::IsError(result)) {
    return result;
  }

  return Error::Success();
}

auto Buffer::UnmapMemory(GraphicsContext &context) -> void {
  if (persistentMapping) {
    return;
  }

  std::lock_guard<std::mutex> lock(
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  vmaUnmapMemory(context.vmaAllocator, this->memory);
  mappedData = nullptr;
}

auto Buffer::UploadLarge(GraphicsContext &context,
                         std::span<const uint8_t> data, // NOLINTNEXTLINE
                         VkDeviceSize offset, VkDeviceSize size) const
    -> Error {

  auto uploadSize = size == VK_WHOLE_SIZE ? data.size() : size;

  if (uploadSize > this->size) {
    return Error::Create(
        "Error uploading data, cannot upload more data than is allocated.");
  }

  // Use staging buffer
  VkBufferCreateInfo stagingBufferInfo = {};
  stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  stagingBufferInfo.size = uploadSize;
  stagingBufferInfo.usage =
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    VkResult result = vmaCreateBuffer(context.vmaAllocator, &stagingBufferInfo,
                                      &stagingAllocInfo, &stagingBuffer,
                                      &stagingMemory, nullptr);
    if (result != VK_SUCCESS) {
      return Error::Create(result);
    }
  }

  void *mapped = nullptr;
  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    VkResult result =
        vmaMapMemory(context.vmaAllocator, stagingMemory, &mapped);
    if (result != VK_SUCCESS) {
      vmaDestroyBuffer(context.vmaAllocator, stagingBuffer, stagingMemory);
      return Error::Create(result);
    }
  }

  std::memcpy(static_cast<uint8_t *>(mapped), data.data(), uploadSize);
  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);
    vmaUnmapMemory(context.vmaAllocator, stagingMemory);
  }

  auto *commandBuffer = GetCommandBuffer();

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = offset;
  copyRegion.size = uploadSize;
  vkCmdCopyBuffer(commandBuffer, stagingBuffer, handle, 1, &copyRegion);
  StagingBufferInfo stagingInfo = {};
  stagingInfo.buffer = stagingBuffer;
  stagingInfo.memory = stagingMemory;
  stagingInfo.timelineValue = currentTimelineValue.fetch_add(1) + 1;

  StagingBuffers.emplace_back(stagingInfo);

  return Error::Success();
}

auto Buffer::UploadRing(GraphicsContext &context,
                        std::span<const uint8_t> data, // NOLINTNEXTLINE
                        VkDeviceSize offset, VkDeviceSize size) const -> Error {

  auto uploadSize = size == VK_WHOLE_SIZE ? data.size() : size;

  if (uploadSize > this->size) {
    return Error::Create(
        "Error uploading data, cannot upload more data than is allocated.");
  }

  // Use upload buffer
  auto uploadBuffer = UploadBuffers.at(context.frameIndex);
  size_t &uploadOffset = UploadBufferOffsets.at(context.frameIndex);
  if (uploadBuffer.get() == nullptr ||
      uploadBuffer->sizeInBytes < uploadSize + uploadOffset) {
    size_t newSize = UploadBufferSize;
    while (newSize < uploadSize + uploadOffset) {
      newSize *= 2;
    }

    Graphics::BufferCreationInfo bufferInfo{};
    bufferInfo.size = newSize;
    bufferInfo.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT) |
                       static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    bufferInfo.properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    bufferInfo.persistentMapping = true;
    bufferInfo.debugName = "Upload Buffer";

    auto result = Graphics::Buffer::Create(context, bufferInfo);
    if (Error::IsError(result)) {
      return result.error();
    }

    UploadBuffers.at(context.frameIndex) = result.value();
  }

  uploadBuffer = UploadBuffers.at(context.frameIndex);

  // Copy data to upload buffer
  auto result = uploadBuffer->MapMemory(context);
  if (Error::IsError(result)) {
    return result;
  }

  // NOLINTNEXTLINE, because of pointer arithmetic
  std::memcpy(static_cast<uint8_t *>(uploadBuffer->mappedData) + uploadOffset,
              data.data(), uploadSize);

  uploadBuffer->UnmapMemory(context);

  // Record copy command
  auto *commandBuffer = GetCommandBuffer();

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset = uploadOffset;
  copyRegion.dstOffset = offset;
  copyRegion.size = uploadSize;

  vkCmdCopyBuffer(commandBuffer, uploadBuffer->handle, handle, 1, &copyRegion);
  uploadOffset += uploadSize;

  auto alignment =
      context.deviceProperties.limits.minUniformBufferOffsetAlignment;

  // Align to minUniformBufferOffsetAlignment
  uploadOffset = (uploadOffset + alignment - 1) & ~(alignment - 1);

  return Error::Success();
}

auto Buffer::Upload(GraphicsContext &context,
                    std::span<const uint8_t> data, // NOLINTNEXTLINE
                    VkDeviceSize offset, VkDeviceSize size) -> Error {

  if (((usage)&VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
    return Error::Create(
        "Buffer was not created with TRANSFER_DST usage flag for upload.");
  }

  auto uploadSize = size == VK_WHOLE_SIZE ? data.size() : size;

  if (uploadSize + offset > this->size) {
    return Error::Create(
        "Error uploading data, cannot upload more data than is allocated.");
  }

  if (isStagingBuffer) {
    // We know this will be a large upload,
    // no need for upload buffers as it is a one time upload and we won't be interfering with
    // the GPU reading from the buffer later
    // So we directly map and write to the buffer memory

    auto result = MapMemory(context);
    if (Error::IsError(result)) {
      return result;
    }

    // NOLINTNEXTLINE, because of pointer arithmetic
    std::memcpy(static_cast<uint8_t *>(mappedData) + offset, data.data(),
                uploadSize);

    UnmapMemory(context);

    return Error::Success();
  }

  if (GetIsCurrentlyRendering()) {
    return Error::Create("Cannot upload to buffer while rendering.");
  }

  if (uploadSize > LargeUploadThreshold) {
    auto uploadResult = UploadLarge(context, data, offset, size);
    if (Error::IsError(uploadResult)) {
      return uploadResult;
    }
  } else {
    auto uploadResult = UploadRing(context, data, offset, size);
    if (Error::IsError(uploadResult)) {
      return uploadResult;
    }
  }

  return Error::Success();
}

auto Buffer::Create(GraphicsContext &context, const BufferCreationInfo &info)
    -> Result<Ref<Buffer>> {
  if (info.size == 0) {
    return Error::Unexpected("Cannot create buffer with size 0.");
  }

  if (info.size == VK_WHOLE_SIZE) {
    return Error::Unexpected("Cannot create buffer with size VK_WHOLE_SIZE.");
  }

  auto debugname = Graphics::ContextDebugname + "_" + info.debugName;

  auto buffer = Ref<Buffer>::Make();

  buffer->isStagingBuffer = info.stagingBuffer;
  buffer->persistentMapping = info.persistentMapping;

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

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);
    VkResult result =
        vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                        &buffer->handle, &buffer->memory, nullptr);

    if (result != VK_SUCCESS) {
      return Error::Unexpected(result);
    }
  }

  if (!debugname.empty()) {
    VkDebugUtilsObjectNameInfoEXT debugNameInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(buffer->handle)), // NOLINT
        .pObjectName = debugname.c_str(),
    };
    auto result = Error::Create(
        vkSetDebugUtilsObjectNameEXT(context.device, &debugNameInfo));

    if (Error::IsError(result)) {
      return result.AsUnexpected();
    }

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.vmaAllocator);

      vmaSetAllocationName(context.vmaAllocator, buffer->memory,
                           debugname.c_str());
    }
  } else {
    PrintWarning("No debug name set for buffer.");
  }

  buffer->size = info.size;
  buffer->usage = info.usage;
  buffer->properties = info.properties;

  PrintDebug("Buffer created with handle {}", (void *)buffer->handle);

  VmaAllocationInfo memRequirements;
  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    vmaGetAllocationInfo(context.vmaAllocator, buffer->memory,
                         &memRequirements);
  }
  buffer->sizeInBytes = memRequirements.size;

  if (info.persistentMapping) {
    PrintDebug("Persistently mapping buffer memory.");

    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    VkResult result =
        vmaMapMemory(context.vmaAllocator, buffer->memory, &buffer->mappedData);
    if (result != VK_SUCCESS) {
      return Error::Unexpected(result);
    }
  }

  PrintDebug("Buffer size in bytes: {}", buffer->sizeInBytes);
  buffer->lastUsedTimestamp = GetSemaphoreValue();

  return buffer;
}

auto Buffer::SetData(GraphicsContext &context,
                     const std::span<const uint8_t> &data, // NOLINTNEXTLINE
                     VkDeviceSize offset, VkDeviceSize size) -> Error {

  auto result = Upload(context, data, offset, size);
  if (Error::IsError(result)) {
    return result;
  }

  MarkUse();

  return Error::Success();
}

auto Buffer::ScheduleDestroy() -> void {
  assert(!released);
  assert(handle != nullptr);

  ScheduleDestruction(this);
  released = true;
}

auto Buffer::MarkUse() -> void {
  lastUsedTimestamp = (std::max)(lastUsedTimestamp, GetSemaphoreValue());
}

// NOLINTNEXTLINE
auto Buffer::Clear(GraphicsContext &context, uint32_t value,
                   VkDeviceSize offset, VkDeviceSize size) -> Error {
  auto *commandBuffer = GetCommandBuffer();

  // Must flush, for WaW hazards
  Barrier::UpdateUsage(context, *this,
                       {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .access = VK_ACCESS_TRANSFER_WRITE_BIT});

  vkCmdFillBuffer(commandBuffer, handle, offset, size, value);

  return Error::Success();
}

Buffer::~Buffer() {
  auto *context = GetCurrentGraphicsContext();

  std::lock_guard<std::mutex> lock(
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  if (persistentMapping && mappedData != nullptr) {
    vmaUnmapMemory(context->vmaAllocator, memory);
    mappedData = nullptr;
  }

  vmaDestroyBuffer(context->vmaAllocator, handle, memory);
}

} // namespace Graphics