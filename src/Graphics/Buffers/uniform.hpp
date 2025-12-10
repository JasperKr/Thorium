#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/object.hpp"
#include <cstdint>
struct FrameUniformBufferObject {
public:
  auto SetData(Graphics::GraphicsContext &context,
               const std::span<const uint8_t> &data) -> Error::Error {
    if (data.size() > size) {
      auto result = buffer->Resize(context, static_cast<uint32_t>(data.size()));
      size = static_cast<uint32_t>(data.size());

      if (Error::IsError(result)) {
        return result;
      }
    }

    auto result = buffer->SetData(context, data, offset);

    offset += static_cast<uint32_t>(data.size());

    return result;
  }
  [[nodiscard]] auto GetOffset() const -> uint32_t { return offset; }
  [[nodiscard]] auto GetSize() const -> uint32_t { return size; }
  [[nodiscard]] auto GetBuffer() const -> Ref<Graphics::Buffer> {
    return buffer;
  }

private:
  Ref<Graphics::Buffer> buffer;

  uint32_t size{};
  uint32_t offset{};
};