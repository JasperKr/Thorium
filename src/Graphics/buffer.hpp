#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "graphics.hpp"
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
namespace Graphics {

struct BufferCreationInfo {
  VkDeviceSize size{};
  VkBufferUsageFlags usage{};
  VkMemoryPropertyFlags properties{};

  // Staging buffers are assumed to be used only once and for large uploads
  bool stagingBuffer = false;

  // Persistent mapping keeps the buffer mapped to cpu memory for its entire lifetime
  // Useful for dynamic buffers that are updated frequently, like UBO's
  bool persistentMapping = false;

  // Debug name for the buffer
  std::string debugName;
};

auto FlushBufferUploads(const GraphicsContext &context) -> Error;
auto LoadBufferModule(const GraphicsContext &context) -> Error;
auto UnloadLocalBufferModule(const GraphicsContext &context) -> Error;
auto UnloadBufferModule(const GraphicsContext &context) -> Error;

static const Type bufferType = Type("Internal Buffer");

struct Buffer : Object, Barrier::BarrierSynced {
  Buffer() = default;
  Buffer(const Buffer &) = delete;
  auto operator=(const Buffer &) -> Buffer & = delete;

  Buffer(Buffer &&) noexcept = delete;
  auto operator=(Buffer &&) noexcept -> Buffer & = delete;

  std::mutex mutex;

  VkBuffer handle = VK_NULL_HANDLE;
  VmaAllocation memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  uint64_t sizeInBytes = 0;
  mutable void *mappedData = nullptr;

  uint64_t lastUsedTimestamp{};

  VkMemoryPropertyFlags properties = 0;
  VkBufferUsageFlags usage = 0;

  // Indicates if this is a staging buffer, meaning it is used for temporary uploads
  bool isStagingBuffer = false;

  // Indicates if this buffer is persistently mapped, can be used to optimize frequent updates
  bool persistentMapping = false;

  // Safety flag to prevent double releases
  bool released = false;

  // Set by cleanup function otherwise we'll try to defer destruction again
  bool isDestroyed = false;

  auto GetTimestamp() const -> uint64_t { return lastUsedTimestamp; }
  auto MarkUse() -> void;

  static auto Create(const Graphics::GraphicsContext &context,
                     const Graphics::BufferCreationInfo &info)
      -> Result<Ref<Graphics::Buffer>>;

  // Release the resources for safe automatic destruction later
  auto ScheduleDestroy() -> void override;
  auto UseDeferredDestruction() const -> bool override {
    return GetDeferredDestructionAllowed() && !isDestroyed;
  }

  ~Buffer() override;

  // Set data into the buffer at the given offset
  auto SetData(const GraphicsContext &context,
               const std::span<const uint8_t> &data, VkDeviceSize offset = 0,
               VkDeviceSize size = VK_WHOLE_SIZE) -> Error;

  auto CopyTo(const GraphicsContext &context, Buffer &dstBuffer,
              size_t srcIndex, size_t dstIndex, size_t size) -> Error;

  auto MapMemory(const GraphicsContext &context) -> Error;
  auto UnmapMemory(const GraphicsContext &context) -> void;

  // NOLINTNEXTLINE
  auto Clear(const GraphicsContext &context, uint32_t value,
             VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
      -> Error;

  static auto GetType() -> Type const * { return &bufferType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return Buffer::GetType();
  }

  auto Upload(const GraphicsContext &context, std::span<const uint8_t> data,
              VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
      -> Error;

  auto RegisterUpload() -> void;
  auto UploadLarge(const GraphicsContext &context,
                   std::span<const uint8_t> data, // NOLINTNEXTLINE
                   VkDeviceSize offset, VkDeviceSize size) const -> Error;
  auto UploadRing(const GraphicsContext &context,
                  std::span<const uint8_t> data, // NOLINTNEXTLINE
                  VkDeviceSize offset, VkDeviceSize size) const -> Error;

  std::string debugName;
};
} // namespace Graphics