#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstdint>

constexpr size_t InitialUniformBufferSize = 64L * 1024; // 64 KB

// 16 MB; allows for doubling in size 8x
constexpr size_t MaximumUniformBufferSize = 64L * 1024 * 1024;

namespace Graphics {

struct FrameUniformBufferObject {
public:
  static auto Create(GraphicsContext &context)
      -> Result<FrameUniformBufferObject>;

  void SetData(const Graphics::GraphicsContext &context,
               const std::span<const uint8_t> &data, uint32_t atOffset);

  auto Flush(const Graphics::GraphicsContext &context) -> Result<bool>;

  auto ScheduleDestroy() -> void { buffer.reset(); }

  [[nodiscard]] auto GetOffset() const -> uint32_t { return offset; }
  [[nodiscard]] auto GetSize() const -> uint32_t { return size; }
  [[nodiscard]] auto GetLastFlushSize() const -> uint32_t {
    return lastFlushSize;
  }
  [[nodiscard]] auto GetBuffer() const -> Ref<Graphics::Buffer> {
    return buffer;
  }
  auto NewFrame() -> void { offset = 0; }

private:
  Ref<Graphics::Buffer> buffer;
  std::vector<uint8_t> localData;

  uint32_t size{};
  uint32_t lastFlushSize{};
  uint32_t offset{};
};

extern thread_local std::vector<FrameUniformBufferObject>
    ThreadUniformBuffers; // NOLINT
auto InitializeUniformBufferModule(GraphicsContext &context) -> Error;
auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject &;
auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void;

} // namespace Graphics