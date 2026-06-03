#pragma once

#include "Graphics/buffer.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>

constexpr size_t InitialUniformBufferSize = 64L * 1024; // 64 KB

// 16 MB; allows for doubling in size 8x
constexpr size_t MaximumUniformBufferSize = 64L * 1024 * 1024;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::atomic<int> UniformBufferObjectCount;

namespace Graphics {

struct FrameUniformBufferObject {
public:
  static auto Create(GraphicsContext &context)
      -> Result<FrameUniformBufferObject>;

  auto Write(const Graphics::GraphicsContext &context,
             const std::span<const uint8_t> &data, size_t writeOffset = 0UL)
      -> Result<bool>;

  [[nodiscard]] auto GetOffset() const -> size_t { return offset; }
  [[nodiscard]] auto GetSize() const -> size_t { return buffer->size; }
  [[nodiscard]] auto GetBuffer() const -> Ref<Graphics::Buffer> {
    return buffer;
  }
  auto NewFrame() -> void { offset = 0; }

private:
  Ref<Graphics::Buffer> buffer;

  size_t offset{};
};

extern thread_local std::vector<FrameUniformBufferObject>
    ThreadUniformBuffers; // NOLINT
auto InitializeUniformBufferModule(GraphicsContext &context) -> Error;
auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject &;
auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void;

} // namespace Graphics