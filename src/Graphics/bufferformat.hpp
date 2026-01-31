#pragma once

#include "Graphics/reflect.hpp"
#include <cassert>
#include <cstdint>
#include <vector>
#define VK_NO_PROTOTYPES
#include "format.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics {

struct BufferComponent {
  ResourceKey name;
  uint32_t offset;
  VkFormat format;
};

struct BufferFormat {
  explicit BufferFormat(std::vector<BufferComponent> components);

  BufferFormat() = default;
  BufferFormat(const BufferFormat &other) = default;
  BufferFormat(BufferFormat &&other) noexcept = default;
  auto operator=(const BufferFormat &other) -> BufferFormat & = default;
  auto operator=(BufferFormat &&other) noexcept -> BufferFormat & = default;
  ~BufferFormat() = default;

public:
  [[nodiscard]] auto GetComponents() const
      -> const std::vector<BufferComponent> & {
    return Components;
  }

  // Assumes components are tightly packed or padded correctly
  [[nodiscard]] auto GetSize() const -> size_t;

  auto operator==(const BufferFormat &other) const -> bool;

  auto operator!=(const BufferFormat &other) const -> bool {
    return !(*this == other);
  }

  [[nodiscard]] auto GetHash() const -> size_t;

  // Query the format at a given component offset
  // Useful for writing one component at a time
  // for example:
  // vec4, uvec4, float, uint will give:
  // vec4, vec4, vec4, vec4, uvec4, uvec4, uvec4, uvec4, float, uint
  [[nodiscard]] auto FormatAt(size_t componentOffset) const -> VkFormat {
    componentOffset %= FormatsAtOffsets.size();
    return Components[componentOffset].format;
  }

private:
  std::vector<BufferComponent> Components;
  std::vector<VkFormat> FormatsAtOffsets;
};

struct BufferFormatHash {
  auto operator()(const BufferFormat &format) const noexcept -> size_t {
    return format.GetHash();
  }
};

} // namespace Graphics