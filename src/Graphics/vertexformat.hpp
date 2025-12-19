#pragma once

#include "Graphics/hash.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics {

static auto VkFormatSize(VkFormat format) -> uint32_t {
  const int floatSize = 4;
  const int intSize = 4;
  const int shortSize = 2;
  const int byteSize = 1;

  switch (format) {
  // Float formats
  case VK_FORMAT_R32_SFLOAT:
    return floatSize;
  case VK_FORMAT_R32G32_SFLOAT:
    return floatSize * 2;
  case VK_FORMAT_R32G32B32_SFLOAT:
    return floatSize * 3;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return floatSize * 4;
  case VK_FORMAT_R16_SFLOAT:
    return shortSize;
  case VK_FORMAT_R16G16_SFLOAT:
    return floatSize;
  case VK_FORMAT_R16G16B16_SFLOAT:
    return shortSize * 3;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return shortSize * 4;

  // Unsigned int formats
  case VK_FORMAT_R8_UINT:
    return byteSize * 1;
  case VK_FORMAT_R8G8_UINT:
    return byteSize * 2;
  case VK_FORMAT_R8G8B8_UINT:
    return byteSize * 3;
  case VK_FORMAT_R8G8B8A8_UINT:
    return byteSize * 4;
  case VK_FORMAT_R16_UINT:
    return shortSize * 1;
  case VK_FORMAT_R16G16_UINT:
    return shortSize * 2;
  case VK_FORMAT_R16G16B16_UINT:
    return shortSize * 3;
  case VK_FORMAT_R16G16B16A16_UINT:
    return shortSize * 4;
  case VK_FORMAT_R32_UINT:
    return intSize;
  case VK_FORMAT_R32G32_UINT:
    return intSize * 2;
  case VK_FORMAT_R32G32B32_UINT:
    return intSize * 3;
  case VK_FORMAT_R32G32B32A32_UINT:
    return intSize * 4;

  // Signed int formats
  case VK_FORMAT_R8_SINT:
    return byteSize * 1;
  case VK_FORMAT_R8G8_SINT:
    return byteSize * 2;
  case VK_FORMAT_R8G8B8_SINT:
    return byteSize * 3;
  case VK_FORMAT_R8G8B8A8_SINT:
    return byteSize * 4;
  case VK_FORMAT_R16_SINT:
    return shortSize * 1;
  case VK_FORMAT_R16G16_SINT:
    return shortSize * 2;
  case VK_FORMAT_R16G16B16_SINT:
    return shortSize * 3;
  case VK_FORMAT_R16G16B16A16_SINT:
    return shortSize * 4;
  case VK_FORMAT_R32_SINT:
    return intSize;
  case VK_FORMAT_R32G32_SINT:
    return intSize * 2;
  case VK_FORMAT_R32G32B32_SINT:
    return intSize * 3;
  case VK_FORMAT_R32G32B32A32_SINT:
    return intSize * 4;

  // Normalized (packed) formats
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
    return byteSize * 1;
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
    return byteSize * 2;
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SNORM:
    return byteSize * 3;
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SNORM:
    return byteSize * 4;
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
    return shortSize * 1;
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
    return shortSize * 2;
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
    return shortSize * 3;
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
    return shortSize * 4;

  default:
    return 0; // unsupported / unknown
  }
}

struct VertexFormat {
  explicit VertexFormat(
      std::vector<VkVertexInputAttributeDescription> attributes)
      : Attributes(std::move(attributes)) {
    ConstructBindings();
  }

  VertexFormat() = default;
  VertexFormat(const VertexFormat &other) = default;
  VertexFormat(VertexFormat &&other) noexcept = default;
  auto operator=(const VertexFormat &other) -> VertexFormat & = default;
  auto operator=(VertexFormat &&other) noexcept -> VertexFormat & = default;
  ~VertexFormat() = default;

public:
  [[nodiscard]] auto GetAttributes() const
      -> const std::vector<VkVertexInputAttributeDescription> & {
    return Attributes;
  }
  [[nodiscard]] auto GetBindings()
      -> const std::vector<VkVertexInputBindingDescription> & {
    if (!constructedBindings) {
      ConstructBindings();
    }
    return Bindings;
  }

  auto operator==(const VertexFormat &other) const -> bool {
    if (Attributes.size() != other.Attributes.size()) {
      return false;
    }

    for (size_t i = 0; i < Attributes.size(); ++i) {
      if (Attributes[i].location != other.Attributes[i].location ||
          Attributes[i].binding != other.Attributes[i].binding ||
          Attributes[i].format != other.Attributes[i].format ||
          Attributes[i].offset != other.Attributes[i].offset) {
        return false;
      }
    }

    return true;
  }

  auto operator!=(const VertexFormat &other) const -> bool {
    return !(*this == other);
  }

  [[nodiscard]] auto GetHash() const -> size_t {
    Hash::Hasher hasher{};
    for (const auto &attribute : Attributes) {
      hasher.add(std::hash<uint32_t>()(attribute.location));
      hasher.add(std::hash<uint32_t>()(attribute.binding));
      hasher.add(std::hash<uint32_t>()(attribute.format));
      hasher.add(std::hash<uint32_t>()(attribute.offset));
    }
    return hasher.get();
  }

private:
  bool constructedBindings = false;
  std::vector<VkVertexInputAttributeDescription> Attributes;

  void ConstructBindings() {
    std::ranges::sort(
        Attributes,
        [](const VkVertexInputAttributeDescription &first,
           const VkVertexInputAttributeDescription &second) -> bool {
          if (first.binding != second.binding) {
            return first.binding < second.binding;
          }
          return first.location < second.location;
        });

    for (auto &component : Attributes) {
      // Sanity check for invalid binding numbers
      // real max is higher but i expect to use like 2 anyways
      assert(component.binding <= 255 &&
             "Vertex attribute binding exceeds maximum of 255");
      Bindings.resize((std::max)(Bindings.size(),
                                 static_cast<size_t>(component.binding + 1)));

      Bindings[component.binding] = VkVertexInputBindingDescription{
          .binding = component.binding,
          .stride = 0,
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };
    }

    for (auto &component : Attributes) {
      auto formatSize = VkFormatSize(component.format);

      assert(formatSize > 0 &&
             "Vertex attribute has unsupported or unknown format");

      component.offset = Bindings[component.binding].stride;
      Bindings[component.binding].stride += formatSize;
    }

    constructedBindings = true;
  }

  std::vector<VkVertexInputBindingDescription> Bindings;
};

struct VertexFormatHash {
  auto operator()(const VertexFormat &format) const noexcept -> size_t {
    return format.GetHash();
  }
};

enum class VertexFormats : uint8_t {
  Unknown = 0,
  Default,
  Animated,
  DefaultInstanced,
  AnimatedInstanced,
  ImGui,
  Default2D
};

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays, hicpp-avoid-c-arrays)

struct Format_Default {
  float position[3];
  uint32_t normal;  // a2b10g10r10
  uint32_t tangent; // a2b10g10r10
  uint16_t uv[4];   // 2x half
  uint16_t uv2[4];  // 2x half
};

struct Format_Animated {
  float position[3];
  uint32_t normal;  // a2b10g10r10
  uint32_t tangent; // a2b10g10r10
  uint16_t uv[4];   // 2x half
  uint16_t uv2[4];  // 2x half
  float boneWeights[4];
  uint32_t boneIndices[4];
};

struct Format_ImGui {
  float position[2];
  float uv[2];
  uint32_t color; // RGBA8
};

struct Format_Default2D {
  float position[2];
  float uv[2];
  uint32_t color; // RGBA8
};

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays, hicpp-avoid-c-arrays)

struct VertexFormatsHash {
  auto operator()(VertexFormats format) const noexcept -> size_t {
    return static_cast<size_t>(format);
  }
};

} // namespace Graphics