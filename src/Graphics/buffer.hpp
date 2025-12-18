#pragma once

#include "Graphics/graphics.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "graphics.hpp"
#include <cstdint>
#include <span>
#include <unordered_map>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
namespace Graphics {

struct BufferCreationInfo {
  VkDeviceSize size{};
  VkBufferUsageFlags usage{};
  VkMemoryPropertyFlags properties{};
  bool IsStagingBuffer = false;
};

auto FlushBufferUploads(GraphicsContext &context) -> Error::Error;
auto LoadBufferModule(GraphicsContext &context) -> Error::Error;
auto UnloadBufferModule(GraphicsContext &context) -> Error::Error;

struct Buffer : Object {
  VkBuffer handle = VK_NULL_HANDLE;
  VmaAllocation memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  uint64_t sizeInBytes = 0;
  VkBufferUsageFlags usage = 0;
  VkMemoryPropertyFlags properties = 0;
  bool isStagingBuffer = false;

  Buffer() = default;
  Buffer(const Buffer &) = delete;
  auto operator=(const Buffer &) -> Buffer & = delete;

  Buffer(Buffer &&) noexcept = delete;
  auto operator=(Buffer &&) noexcept -> Buffer & = delete;

  std::unordered_map<QueueID, uint64_t> lastUsedTimelineValues;
  bool released{false};

  auto GetTimelineValues() const
      -> const std::unordered_map<QueueID, uint64_t> & {
    return lastUsedTimelineValues;
  }

  auto MarkUse(const QueueID queueID, const uint64_t timelineValue) -> void {
    uint64_t previousValue{};
    if (lastUsedTimelineValues.contains(queueID)) {
      previousValue = lastUsedTimelineValues.at(queueID);
    }

    lastUsedTimelineValues[queueID] = (std::max)(previousValue, timelineValue);
  }

  static auto Create(Graphics::GraphicsContext &context,
                     Graphics::BufferCreationInfo info)
      -> tl::expected<Ref<Graphics::Buffer>, Error::Error>;

  // Release the resources for safe automatic destruction later
  auto ScheduleDestroy() -> bool override;
  auto UseDeferredDestruction() const -> bool override { return true; }

  // Destroy the buffer immediately, use with caution
  auto Destroy(GraphicsContext &context) const -> void;

  ~Buffer() override {
    if (!released) {
      PrintWarning("Buffer destroyed without being queued for destruction!");
      auto *context = GetCurrentGraphicsContext();
      vkQueueWaitIdle(context->graphicsQueue);
      Destroy(*context);
    }
  }

  // Set data into the buffer at the given offset
  auto SetData(GraphicsContext &context, std::span<const uint8_t> data,
               VkDeviceSize offset) -> Error::Error;

  template <typename T> // Set data with span of T
  auto SetData(GraphicsContext &context, std::span<T> data,
               VkDeviceSize offset = 0) -> Error::Error {
    auto byteSpan = // NOLINTNEXTLINE
        std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.data()),
                                 sizeof(T) * data.size());
    return SetData(context, byteSpan, offset);
  }
  template <typename T> // Set data with vector of T
  auto SetData(GraphicsContext &context, const std::vector<T> &data,
               VkDeviceSize offset = 0) -> Error::Error {
    auto byteSpan = // NOLINTNEXTLINE
        std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.data()),
                                 sizeof(T) * data.size());
    return SetData(context, byteSpan, offset);
  }

  template <typename T> // Set data with pointer to array of T
  auto SetData(GraphicsContext &context, const T **data,
               VkDeviceSize offset = 0) -> Error::Error {
    auto byteSpan = // NOLINTNEXTLINE
        std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(&data),
                                 sizeof(T));
    return SetData(context, byteSpan, offset);
  }
};
} // namespace Graphics