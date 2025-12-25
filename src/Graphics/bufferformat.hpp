#pragma once

#include "Graphics/hash.hpp"
#include "Graphics/reflect.hpp"
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
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
  explicit BufferFormat(std::vector<BufferComponent> components)
      : Components(std::move(components)) {
    // calculate offsets
    size_t currentOffset = 0;
    for (auto &component : Components) {
      component.offset = static_cast<uint32_t>(currentOffset);
      currentOffset += Graphics::Format::GetSize(component.format);
    }
  }

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
  [[nodiscard]] auto GetSize() const -> size_t {
    size_t size = 0;
    for (const auto &component : Components) {
      size += Graphics::Format::GetSize(component.format);
    }
    return size;
  }

  auto operator==(const BufferFormat &other) const -> bool {
    if (Components.size() != other.Components.size()) {
      return false;
    }

    for (size_t i = 0; i < Components.size(); ++i) {
      if (Components[i].format != other.Components[i].format ||
          Components[i].name != other.Components[i].name) {
        return false;
      }
    }

    return true;
  }

  auto operator!=(const BufferFormat &other) const -> bool {
    return !(*this == other);
  }

  [[nodiscard]] auto GetHash() const -> size_t {
    Hash::Hasher hasher{};
    for (const auto &attribute : Components) {

      // loop over Resource Key linked list

      auto begin = attribute.name.begin();
      auto end = attribute.name.end();

      for (auto it = begin; it != end; ++it) {
        hasher.add(std::hash<std::string>()(*it));
      }

      hasher.add(std::hash<uint32_t>()(attribute.format));
    }
    return hasher.get();
  }

private:
  std::vector<BufferComponent> Components;
};

struct BufferFormatHash {
  auto operator()(const BufferFormat &format) const noexcept -> size_t {
    return format.GetHash();
  }
};

} // namespace Graphics