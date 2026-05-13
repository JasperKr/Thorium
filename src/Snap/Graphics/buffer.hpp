#pragma once

#include "Graphics/barrier.hpp"
#include "Libraries/vma.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>

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

const static Type LuaBufferReadbackType = Type("BufferReadback");

struct BufferReadback : Object {
  static auto GetType() -> Type const * { return &LuaBufferReadbackType; }
  auto GetInstanceType() const -> Type const * override {
    return BufferReadback::GetType();
  }

  Ref<Data::ByteData> data;
  bool completed = false;
  Error error = Error::Success();

  std::mutex mutex;
  std::condition_variable conditionVar;
};

auto FlushBufferUploads(const GraphicsContext &context) -> Error;
auto LoadBufferModule(const GraphicsContext &context) -> Error;
auto UnloadLocalBufferModule(const GraphicsContext &context) -> Error;
auto UnloadBufferModule(const GraphicsContext &context) -> Error;

static const Type LuaInternalBufferType = Type("InternalBuffer");

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
  auto UseDeferredDestruction() const -> bool override;

  ~Buffer() override;

  // Set data into the buffer at the given offset
  auto SetData(const GraphicsContext &context,
               const std::span<const uint8_t> &data, VkDeviceSize offset = 0,
               VkDeviceSize size = VK_WHOLE_SIZE) -> Error;

  template <typename T>
  auto SetData(const GraphicsContext &context, const std::span<T> &data,
               VkDeviceSize offset = 0) -> Error {
    return SetData(
        context,
        std::span<const uint8_t>( // NOLINTNEXTLINE
            reinterpret_cast<const uint8_t *>(data.data()), data.size_bytes()),
        offset, data.size_bytes());
  }

  auto CopyTo(const GraphicsContext &context, Buffer &dstBuffer,
              size_t srcIndex, size_t dstIndex, size_t size) -> Error;

  auto CopyTo(const GraphicsContext &context, Texture &dstTexture,
              VkBufferImageCopy region) -> Error;

  auto MapMemory(const GraphicsContext &context) -> Error;
  auto UnmapMemory(const GraphicsContext &context) -> void;

  // NOLINTNEXTLINE
  auto Clear(const GraphicsContext &context, uint32_t value,
             VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE)
      -> Error;

  static auto GetType() -> Type const * { return &LuaInternalBufferType; }

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

  auto Readback(const GraphicsContext &context, VkDeviceSize offset = 0,
                VkDeviceSize size = VK_WHOLE_SIZE,
                const Ref<Data::ByteData> &output = Ref<Data::ByteData>())
      -> Result<Ref<BufferReadback>>;

  std::string debugName;
};
} // namespace Graphics