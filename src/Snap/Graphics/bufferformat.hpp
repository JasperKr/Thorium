#pragma once

#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics {

using BufferTreeComponent = std::variant<VkFormat, struct BufferFormat>;

struct BufferComponent;

enum class Standard : uint8_t {
  Std430,
  Std140,
};

struct BufferFormat {
  explicit BufferFormat(std::vector<BufferComponent> components,
                        Standard std = Standard::Std430);

  BufferFormat() = default;
  BufferFormat(const BufferFormat &other) = default;
  BufferFormat(BufferFormat &&other) noexcept = default;
  auto operator=(const BufferFormat &other) -> BufferFormat & = default;
  auto operator=(BufferFormat &&other) noexcept -> BufferFormat & = default;
  ~BufferFormat() = default;

private:
  auto FlattenComponentTree() -> void;

  auto CalculateStride(Standard std) -> size_t;
  auto Offset(size_t offset) -> void;

  [[nodiscard]] auto FindComponent(ResourceKey::const_iterator iterator,
                                   ResourceKey::const_iterator end) const
      -> std::optional<BufferComponent>;

public:
  [[nodiscard]] auto GetVkComponents() -> const std::vector<VkFormat> & {
    if (FlatComponents.empty()) {
      FlattenComponentTree();
    }

    return FlatComponents;
  }

  [[nodiscard]] auto GetComponents() const
      -> const std::vector<BufferComponent> & {
    return Components;
  }

  // Get the calculated stride of the format
  [[nodiscard]] auto GetStride() const -> size_t { return stride; }

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
  [[nodiscard]] auto FormatAt(size_t componentOffset) -> VkFormat;

  [[nodiscard]] auto ToString(int indentation = 0) const -> std::string;

private:
  // Definition tree
  std::vector<BufferComponent> Components;

  // The list of components in the format, inorder flattended structure tree
  std::vector<VkFormat> FlatComponents;

  // The stride of the element, including any padding between components
  size_t stride = 0;
  uint32_t alignment{};

  // Stride and alignment initialized flag
  bool initialized = false;
};

struct BufferComponent {
  std::string name;
  uint32_t offset;
  BufferTreeComponent format;
  size_t arraySize = 1;
  bool isMatrix;

  // Vertex format only
  uint32_t location;
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

} // namespace Graphics