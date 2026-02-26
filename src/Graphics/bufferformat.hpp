#pragma once

#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics {

struct BufferComponent {
  ResourceKey name;
  uint32_t offset;
  VkFormat format;
  size_t arraySize = 1; // for arrays of components like float[4] or matrices
};

enum class Standard : uint8_t {
  Std430,
  Std140,
};

using Component = std::variant<VkFormat, struct BufferFormat>;

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
  [[nodiscard]] auto GetElementStride() const -> size_t;

  // Get the stride of the format according to a given standard, which may add padding between components
  [[nodiscard]] auto GetStride(Standard std) const -> size_t;

  // Get the offset of a component by name
  [[nodiscard]] auto GetComponentOffset(const ResourceKey &name) const
      -> Result<size_t>;

  // Get the offset of a component by index
  [[nodiscard]] auto GetComponentOffset(size_t index) const -> size_t;

  auto operator==(const BufferFormat &other) const -> bool;

  auto operator!=(const BufferFormat &other) const -> bool {
    return !(*this == other);
  }

  [[nodiscard]] auto GetHash() const -> size_t;

  // Query the format at a given component offset
  // Useful for writing one component at a time
  // for example:
  // vec4, uvec4, float, uint, uint8[2] will give:
  // vec4, vec4, vec4, vec4, uvec4, uvec4, uvec4, uvec4, float, uint, uint8, uint8
  [[nodiscard]] auto FormatAt(size_t componentOffset) const -> VkFormat {
    componentOffset %= FormatsAtIndices.size();
    return Components[componentOffset].format;
  }

private:
  // The list of components in the format, in declaration order
  std::vector<BufferComponent> Components;

  // The format of each component at the corresponding offset in the element, expanded for arrays
  std::vector<VkFormat> FormatsAtIndices;

  // byte offset of each component within the element
  std::vector<size_t> ComponentOffsets;

  // The stride of the element, including any padding between components
  size_t stride = 0;
};

struct BufferFormatHash {
  auto operator()(const BufferFormat &format) const noexcept -> size_t {
    return format.GetHash();
  }
};

// Get the size of a format
auto GetAlignment(VkFormat format) -> Result<size_t>;

// Get the size of a buffer component according to the given standard
auto GetAlignment(const BufferComponent &component) -> Result<size_t>;

// Get the alignment of an element according to the given standard at the given index
auto GetElementStride(const std::vector<BufferComponent> &formats, Standard std,
                      std::vector<size_t> &offsets) -> Result<size_t>;

} // namespace Graphics