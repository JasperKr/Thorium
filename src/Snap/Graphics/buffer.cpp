#include "buffer.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/resource.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Graphics/snapshot.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "graphics.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <thread>
#include <vector>

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

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto NameBuffer(VkBuffer buffer, VmaAllocation memory,
                       const std::string &name) -> Error {
  auto *contextPtr = GetCurrentGraphicsContext();

  if (contextPtr == nullptr) {
    return Error::Create(
        "Attempted to name a buffer, but no graphics context is current.");
  }

  if (name.empty()) {
    return Error::Create("Buffer name cannot be empty.");
  }

  if (buffer == VK_NULL_HANDLE) {
    return Error::Create("Buffer handle is null.");
  }

  if (memory == VK_NULL_HANDLE) {
    return Error::Create("Buffer memory handle is null.");
  }

  auto &context = *contextPtr;

  VkDebugUtilsObjectNameInfoEXT debugNameInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_BUFFER,
      .objectHandle =
          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(buffer)), // NOLINT
      .pObjectName = name.c_str(),
  };
  CHECK_ERR(Error::Create(
      vkSetDebugUtilsObjectNameEXT(context.device, &debugNameInfo)));

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    vmaSetAllocationName(context.vmaAllocator, memory, name.c_str());
  }

  return Error::Success();
}

// To be called at the end of each frame that uses uploads
auto FlushBufferUploads(const GraphicsContext &context) -> Error {
  // Reset upload buffer offsets for next frame
  for (auto &offset : UploadBufferOffsets) {
    offset = 0;
  }

  return Error::Success();
}

auto Buffer::MapMemory(const GraphicsContext &context) -> Error {
  if (persistentMapping) {
    if (mappedData != nullptr) {
      return Error::Success();
    }

    return Error::Create("Buffer was created with persistent mapping, but "
                         "mappedData is null.");
  }

  std::lock_guard<std::mutex> lock(
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  CHECK_NEW_ERR(vmaMapMemory(context.vmaAllocator, this->memory, &mappedData));

  return Error::Success();
}

auto Buffer::UnmapMemory(const GraphicsContext &context) -> void {
  if (persistentMapping) {
    return;
  }

  std::lock_guard<std::mutex> lock(
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  vmaUnmapMemory(context.vmaAllocator, this->memory);
  mappedData = nullptr;
}

auto Buffer::UploadLarge(const GraphicsContext &context,
                         std::span<const uint8_t> data, // NOLINTNEXTLINE
                         VkDeviceSize offset, VkDeviceSize size) const
    -> Error {
  ZoneScoped;

  auto uploadSize = size == VK_WHOLE_SIZE ? data.size() : size;

  if (uploadSize > this->size) {
    return Error::Create("Error uploading large data, cannot upload more data "
                         "than is allocated.");
  }

  static BufferCreationInfo stagingBufferInfo{
      .usage =
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      .stagingBuffer = true,
      .persistentMapping = false,
      .debugName = "Staging Buffer",
  };
  stagingBufferInfo.size = uploadSize;

  auto stagingBuffer = CHECK_RES(Buffer::Create(context, stagingBufferInfo));
  CHECK_ERR(stagingBuffer->MapMemory(context));

  std::memcpy(static_cast<uint8_t *>(stagingBuffer->mappedData), data.data(),
              uploadSize);
  stagingBuffer->UnmapMemory(context);

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for buffer upload.");
  }

  DynamicRendering::EndRendering(context);

  Barrier::UpdateUsage(context, *this,
                       {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .access = VK_ACCESS_TRANSFER_WRITE_BIT});

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = offset;
  copyRegion.size = uploadSize;
  vkCmdCopyBuffer(commandBuffer, stagingBuffer->handle, handle, 1, &copyRegion);

  return Error::Success();
}

auto Buffer::UploadRing(const GraphicsContext &context,
                        std::span<const uint8_t> data, // NOLINTNEXTLINE
                        VkDeviceSize offset, VkDeviceSize size) const -> Error {
  ZoneScoped;

  auto uploadSize = size == VK_WHOLE_SIZE ? data.size() : size;

  if (uploadSize > this->size) {
    return Error::Create("Error uploading ring data, cannot upload more data "
                         "than is allocated.");
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

    UploadBuffers.at(context.frameIndex) =
        CHECK_RES(Graphics::Buffer::Create(context, bufferInfo));
  }

  uploadBuffer = UploadBuffers.at(context.frameIndex);

  // Copy data to upload buffer
  CHECK_ERR(uploadBuffer->MapMemory(context));

  // NOLINTNEXTLINE, because of pointer arithmetic
  std::memcpy(static_cast<uint8_t *>(uploadBuffer->mappedData) + uploadOffset,
              data.data(), uploadSize);

  uploadBuffer->UnmapMemory(context);

  // Record copy command
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for buffer upload.");
  }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset = uploadOffset;
  copyRegion.dstOffset = offset;
  copyRegion.size = uploadSize;

  Barrier::UpdateUsage(context, *this,
                       {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .access = VK_ACCESS_TRANSFER_WRITE_BIT});

  DynamicRendering::EndRendering(context);
  vkCmdCopyBuffer(commandBuffer, uploadBuffer->handle, handle, 1, &copyRegion);
  uploadOffset += uploadSize;

  auto alignment =
      context.deviceProperties.limits.minUniformBufferOffsetAlignment;

  // Align to minUniformBufferOffsetAlignment
  uploadOffset = (uploadOffset + alignment - 1) & ~(alignment - 1);

  return Error::Success();
}

auto Buffer::Upload(const GraphicsContext &context,
                    std::span<const uint8_t> data, // NOLINTNEXTLINE
                    VkDeviceSize offset, VkDeviceSize size) -> Error {
  ZoneScoped;

  auto uploadSize = size == VK_WHOLE_SIZE ? data.size() : size;

  if (uploadSize + offset > this->size) {
    return Error::Create(
        "Error uploading data, cannot upload more data than is allocated.");
  }

#if Enable_Snapshots
  Snapshot::CaptureEvent(Snapshot::BufferUploadEvent(
      handle, memory, offset, uploadSize, // NOLINTNEXTLINE
      std::vector<uint8_t>(data.data(), data.data() + uploadSize)));
#endif

  if (isStagingBuffer) {
    ZoneScopedN("Direct upload to staging buffer");
    // We know this will be a large upload,
    // no need for upload buffers as it is a one time upload and we won't be interfering with
    // the GPU reading from the buffer later
    // So we directly map and write to the buffer memory

    CHECK_ERR(MapMemory(context));

    // NOLINTNEXTLINE, because of pointer arithmetic
    std::memcpy(static_cast<uint8_t *>(mappedData) + offset, data.data(),
                uploadSize);

    UnmapMemory(context);

    return Error::Success();
  }

  if (((usage)&VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
    return Error::Create(
        "Buffer was not created with TRANSFER_DST usage flag for upload.");
  }

  if (uploadSize > LargeUploadThreshold) {
    CHECK_ERR(UploadLarge(context, data, offset, size));

  } else {
    CHECK_ERR(UploadRing(context, data, offset, size));
  }

  return Error::Success();
}

auto Buffer::Create(const GraphicsContext &context,
                    const BufferCreationInfo &info) -> Result<Ref<Buffer>> {
  ZoneScoped;

  if (info.size == 0) {
    return Error::Unexpected("Cannot create buffer with size 0.");
  }

  if (info.size == VK_WHOLE_SIZE) {
    return Error::Unexpected("Cannot create buffer with size VK_WHOLE_SIZE.");
  }

  auto buffer = Ref<Buffer>::Make();

  buffer->debugName = info.debugName;

  buffer->isStagingBuffer = info.stagingBuffer;
  buffer->persistentMapping = info.persistentMapping;

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = info.size;
  bufferInfo.usage = info.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (info.stagingBuffer || info.persistentMapping) {
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
  }

  allocInfo.requiredFlags = info.properties;
  allocInfo.flags = 0;

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    CHECK_NEW_ERR(vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                                  &buffer->handle, &buffer->memory, nullptr));
  }

  if (!buffer->debugName.empty()) {
    auto err = NameBuffer(buffer->handle, buffer->memory, buffer->debugName);
    if (Error::IsError(err)) {
      PrintWarning("Failed to set debug name for buffer: {}", err.message);
    }
  } else {
    PrintWarning("No debug name set for buffer.");
  }

  buffer->size = info.size;
  buffer->usage = info.usage;
  buffer->properties = info.properties;

  Buffer::TotalAllocatedMemory += buffer->sizeInBytes;

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

    CHECK_NEW_ERR(vmaMapMemory(context.vmaAllocator, buffer->memory,
                               &buffer->mappedData));
  }

  PrintDebug("Buffer size in bytes: {}", buffer->sizeInBytes);
  buffer->lastUsedTimestamp = GetSemaphoreValue();

  return buffer;
}

auto Buffer::SetData(const GraphicsContext &context,
                     const std::span<const uint8_t> &data, // NOLINTNEXTLINE
                     VkDeviceSize offset, VkDeviceSize size) -> Error {

  CHECK_ERR(Upload(context, data, offset, size));

  MarkUse();

  return Error::Success();
}

auto Buffer::CopyTo(const GraphicsContext &context,
                    Buffer &dstBuffer, // NOLINTNEXTLINE
                    size_t srcIndex, size_t dstIndex, size_t size) -> Error {
  if (((usage)&VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0) {
    return Error::Create(
        "Source buffer was not created with TRANSFER_SRC usage "
        "flag for copy.");
  }

  if (((dstBuffer.usage) & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
    return Error::Create("Destination buffer was not created with TRANSFER_DST "
                         "usage flag for copy.");
  }

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for buffer copy.");
  }

  if (srcIndex + size > this->size) {
    return Error::Create("Source index and size exceed buffer size.");
  }

  if (dstIndex + size > dstBuffer.size) {
    return Error::Create("Destination index and size exceed buffer size.");
  }

  if (this == &dstBuffer) { // Check for overlapping self-copies
    if ((srcIndex < dstIndex && srcIndex + size > dstIndex) ||
        (dstIndex < srcIndex && dstIndex + size > srcIndex)) {
      return Error::Create("source and destination regions "
                           "overlap when copying within the same buffer.");
    }
  }

  DynamicRendering::EndRendering(context);

  if (handle != dstBuffer.handle) {
    Barrier::UpdateUsage(context, *this,
                         {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                          .access = VK_ACCESS_TRANSFER_READ_BIT});
    Barrier::UpdateUsage(context, dstBuffer,
                         {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                          .access = VK_ACCESS_TRANSFER_WRITE_BIT});
  } else {
    // If copying within the same buffer, we only need to set the barrier once with both read and write access
    Barrier::UpdateUsage(
        context, *this,
        {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
         .access = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT});
  }

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset = srcIndex;
  copyRegion.dstOffset = dstIndex;
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, handle, dstBuffer.handle, 1, &copyRegion);

  MarkUse();
  dstBuffer.MarkUse();

  return Error::Success();
}

auto Buffer::CopyTo(const GraphicsContext &context, Texture &dstTexture,
                    VkBufferImageCopy region) -> Error {
  if (((dstTexture.usage) & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
    return Error::Create("Destination texture was not created with "
                         "TRANSFER_DST usage flag for copy.");
  }

  auto *commandBuffer = GetCommandBuffer();
  if (commandBuffer == nullptr) {
    return Error::Create(
        "Failed to get command buffer for buffer to image copy.");
  }

  if ((usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0) {
    return Error::Create(
        "Source buffer was not created with TRANSFER_SRC usage flag for copy.");
  }

  DynamicRendering::EndRendering(context);

  CHECK_ERR(dstTexture.UseAsTransferDst(context));

  vkCmdCopyBufferToImage(commandBuffer, handle, dstTexture.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  MarkUse();
  dstTexture.MarkUse();

  return Error::Success();
}

auto Buffer::Grow(const GraphicsContext &context, size_t newSize)
    -> Result<Ref<Buffer>> {

  uint32_t requiredUsageFlags =
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  if ((usage & requiredUsageFlags) != requiredUsageFlags) {
    return Error::Create("Buffer must have TRANSFER_DST and TRANSFER_SRC usage "
                         "flags to be resized.");
  }

  if (newSize == size) {
    return {};
  }

  if (newSize < size) {
    return Error::Create("Don't grow buffers to smaller sizes.");
  }

  if (isStagingBuffer) {
    return Error::Create("Cannot grow staging buffers, as they are meant to be "
                         "temporary and short-lived.");
  }

  auto newBuffer = CHECK_RES(
      Buffer::Create(context, BufferCreationInfo{
                                  .size = newSize,
                                  .usage = usage,
                                  .properties = properties,
                                  .persistentMapping = persistentMapping,
                                  .debugName = debugName,
                              }));

  CHECK_ERR(CopyTo(context, *newBuffer, 0, 0, size));

  return newBuffer;
}

auto Buffer::ScheduleDestroy() -> void {
  if (released) {
    return;
  }
  assert(handle != nullptr);

  ScheduleDestruction(this);
  released = true;
}

auto Buffer::MarkUse() -> void {
  lastUsedTimestamp = (std::max)(lastUsedTimestamp, GetSemaphoreValue());
}

// NOLINTNEXTLINE
auto Buffer::Clear(const GraphicsContext &context, uint32_t value,
                   VkDeviceSize offset, VkDeviceSize size) -> Error {
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for buffer clear.");
  }

  // Must flush, for WaW hazards
  Barrier::UpdateUsage(context, *this,
                       {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .access = VK_ACCESS_TRANSFER_WRITE_BIT});

  vkCmdFillBuffer(commandBuffer, handle, offset, size, value);

  return Error::Success();
}

auto Buffer::Readback(const GraphicsContext &context,
                      VkDeviceSize offset, // NOLINT
                      VkDeviceSize size, const Ref<Data::ByteData> &output)
    -> Result<Ref<BufferReadback>> {
  if (((usage)&VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0) {
    return Error::Unexpected(
        "Buffer was not created with TRANSFER_SRC usage flag for readback.");
  }

  const auto uploadSize = size == VK_WHOLE_SIZE ? this->size : size;

  if (uploadSize + offset > this->size) {
    return Error::Unexpected(
        "Error reading back data, cannot read more data than is allocated.");
  }

  if (output.isValid() && uploadSize > output->GetSize()) {
    return Error::Unexpected("Error reading back data, output buffer is too "
                             "small for requested readback size.");
  }

  // Use staging buffer
  VkBufferCreateInfo stagingBufferInfo = {};
  stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  stagingBufferInfo.size = uploadSize;
  stagingBufferInfo.usage =
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  // See: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
  VmaAllocationCreateInfo stagingAllocInfo = {};
  stagingAllocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
  stagingAllocInfo.flags = 0;
  VkBuffer stagingBuffer = nullptr;
  VmaAllocation stagingMemory = nullptr;

  {
    std::lock_guard<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    CHECK_NEW_ERR(vmaCreateBuffer(context.vmaAllocator, &stagingBufferInfo,
                                  &stagingAllocInfo, &stagingBuffer,
                                  &stagingMemory, nullptr));
  }

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Unexpected(
        "Failed to get command buffer for buffer readback.");
  }

  DynamicRendering::EndRendering(context);

  Barrier::UpdateUsage(context, *this,
                       {.stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .access = VK_ACCESS_TRANSFER_READ_BIT});

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset = offset;
  copyRegion.dstOffset = 0;
  copyRegion.size = uploadSize;
  vkCmdCopyBuffer(commandBuffer, handle, stagingBuffer, 1, &copyRegion);

  MarkUse();
  auto timelineValue = GetSemaphoreValue();

  auto bufferReadback = Ref<BufferReadback>::Make();
  if (output.isValid()) {
    bufferReadback->data = output;
  } else {
    bufferReadback->data = Ref<Data::ByteData>::Make(uploadSize);
  }

  auto readbackThread = std::thread([context, stagingBuffer, stagingMemory,
                                     timelineValue, uploadSize,
                                     bufferReadback]() -> void {
    void *mapped = nullptr;
    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.vmaAllocator);

      VkResult result =
          vmaMapMemory(context.vmaAllocator, stagingMemory, &mapped);
      if (result != VK_SUCCESS) {
        vmaDestroyBuffer(context.vmaAllocator, stagingBuffer, stagingMemory);
        bufferReadback->error = Error::Create(result);

        {
          std::lock_guard lock(bufferReadback->mutex);
          bufferReadback->completed = true;
          bufferReadback->conditionVar.notify_all();
        }

        return;
      }
    }

    {
      std::unique_lock<std::mutex> lock(timelineCompletionMutex);
      timelineCompletionCV.wait(
          lock, [&]() -> bool { return !IsInUse(timelineValue); });
    }

    std::memcpy(bufferReadback->data->GetData(), mapped, uploadSize);

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.vmaAllocator);
      vmaUnmapMemory(context.vmaAllocator, stagingMemory);
      vmaDestroyBuffer(context.vmaAllocator, stagingBuffer, stagingMemory);
    }

    bufferReadback->completed = true;
    {
      std::lock_guard lock(bufferReadback->mutex);
      bufferReadback->conditionVar.notify_all();
    }
  });

  readbackThread.detach();

  return bufferReadback;
}

auto Buffer::UseDeferredDestruction() const -> bool {
  return GetDeferredDestructionAllowed() && !isDestroyed;
}

Buffer::~Buffer() {
  auto *context = GetCurrentGraphicsContext();

  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  if (persistentMapping && mappedData != nullptr) {
    vmaUnmapMemory(context->vmaAllocator, memory);
    mappedData = nullptr;
  }

  vmaDestroyBuffer(context->vmaAllocator, handle, memory);
  Buffer::TotalAllocatedMemory -= sizeInBytes;
}

std::atomic<VkDeviceSize> Buffer::TotalAllocatedMemory{};

} // namespace Graphics