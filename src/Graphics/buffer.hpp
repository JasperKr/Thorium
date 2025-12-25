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

  // Staging buffers are assumed to be used only once and for large uploads
  bool IsStagingBuffer = false;

  // Persistent mapping keeps the buffer mapped to cpu memory for its entire lifetime
  // Useful for dynamic buffers that are updated frequently, like UBO's
  bool PersistentMapping = false;
};

auto FlushBufferUploads(GraphicsContext &context) -> Error::Error;
auto LoadBufferModule(GraphicsContext &context) -> Error::Error;
auto UnloadBufferModule(GraphicsContext &context) -> Error::Error;

static const Type bufferType = Type("Buffer");

struct Buffer : Object {
  VkBuffer handle = VK_NULL_HANDLE;
  VmaAllocation memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  uint64_t sizeInBytes = 0;
  VkBufferUsageFlags usage = 0;
  VkMemoryPropertyFlags properties = 0;
  bool isStagingBuffer = false;
  bool persistentMapping = false;
  void *mappedData = nullptr;

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

  auto MarkUse(QueueID queueID, uint64_t timelineValue) -> void;

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
               VkDeviceSize offset, VkDeviceSize size) -> Error::Error;

  template <typename T> // Set data with span of T
  auto SetData(GraphicsContext &context, std::span<T> data, // NOLINTNEXTLINE
               VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
      -> Error::Error {

    auto uploadSize = size == VK_WHOLE_SIZE ? sizeof(T) * data.size() : size;

    auto byteSpan = // NOLINTNEXTLINE
        std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.data()),
                                 uploadSize);
    return SetData(context, byteSpan, offset, size);
  }
  template <typename T> // Set data with vector of T
  auto SetData(GraphicsContext &context,
               const std::vector<T> &data, // NOLINTNEXTLINE
               VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
      -> Error::Error {

    auto uploadSize = size == VK_WHOLE_SIZE ? sizeof(T) * data.size() : size;

    auto byteSpan = // NOLINTNEXTLINE
        std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.data()),
                                 uploadSize);
    return SetData(context, byteSpan, offset, size);
  }

  auto MapMemory(GraphicsContext &context) -> Error::Error;
  auto UnmapMemory(GraphicsContext &context) -> void;

  // NOLINTNEXTLINE
  void Clear(GraphicsContext &context, uint32_t value, VkDeviceSize offset = 0,
             VkDeviceSize size = VK_WHOLE_SIZE);

  static auto GetType() -> Type const * { return &bufferType; }

private:
  auto Upload(GraphicsContext &context, std::span<const uint8_t> data,
              VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
      -> Error::Error;
};
} // namespace Graphics