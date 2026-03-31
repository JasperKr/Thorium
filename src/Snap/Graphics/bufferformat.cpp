#include "bufferformat.hpp"
#include "Graphics/format.hpp"
#include "Graphics/hash.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics {
BufferFormat::BufferFormat(std::vector<BufferComponent> components,
                           Standard std)
    : Components(std::move(components)) {

  PrintAlways("Creating buffer format with {} components", Components.size());

  size_t offset = 0;
  CalculateStride(std, offset);
}

auto BufferFormat::GetComponentOffset(size_t componentIndex) const -> size_t {
  if (componentIndex >= Components.size()) {
    return 0;
  }

  return Components[componentIndex].offset;
}

auto BufferFormat::FindComponent(ResourceKey::const_iterator iterator,
                                 ResourceKey::const_iterator end) const
    -> std::optional<BufferComponent> {

  if (std::next(iterator) == end) {
    return std::nullopt;
  }

  const auto &name = *iterator;

  for (const auto &Component : Components) {
    if (Component.name == name) {
      if (std::holds_alternative<BufferFormat>(Component.format)) {
        const auto &format = std::get<BufferFormat>(Component.format);
        return format.FindComponent(std::next(iterator), end);
      }

      return Component;
    }
  }

  return std::nullopt;
}

auto BufferFormat::GetComponentOffset(const ResourceKey &name) const
    -> Result<size_t> {
  auto component = FindComponent(name.begin(), name.end());

  if (component.has_value()) {
    return component->offset;
  }

  return Error::Unexpected(
      "Buffer format does not contain component with name: " +
      ResourceKeyToString(name));
}

auto BufferFormat::operator==(const BufferFormat &other) const -> bool {
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

[[nodiscard]] auto BufferFormat::GetHash() const -> size_t {
  Hash::Hasher hasher{};
  for (const auto &attribute : Components) {

    // loop over Resource Key linked list

    auto begin = attribute.name.begin();
    auto end = attribute.name.end();

    for (auto it = begin; it != end; ++it) {
      hasher.add(std::hash<std::string>()(&*it));
    }

    // hasher.add(std::hash<uint32_t>()(attribute.format));
    if (std::holds_alternative<VkFormat>(attribute.format)) {
      hasher.add(std::hash<uint32_t>()(std::get<VkFormat>(attribute.format)));
    } else {
      hasher.add(std::get<BufferFormat>(attribute.format).GetHash());
    }
  }
  return hasher.get();
}

auto GetAlignment(VkFormat format) -> Result<size_t> {
  switch (format) {
  // 32-bit vec3/vec4: alignment = 4N = 16
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
    return 16; // NOLINT
  // 32-bit vec2: alignment = 2N = 8
  // 16-bit vec3/vec4: alignment = 4N = 8
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:

  case VK_FORMAT_R16G16B16A16_SFLOAT:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_SINT:
    return 8; // NOLINT
  // 32-bit scalar: alignment = N = 4
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
  // 16-bit vec2: alignment = 2N = 4
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_SINT:
  // 8-bit vec3/vec4: alignment = 4N = 4
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8_SINT:
    return 4; // NOLINT
  // 16-bit scalar: alignment = N = 2
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_SINT:
  // 8-bit vec2: alignment = 2N = 2
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_SINT:
    return 2; // NOLINT
  // 8-bit scalar: alignment = N = 1
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_SINT:
    return 1; // NOLINT
  default:
    return Error::Unexpected("Unsupported format for alignment: " +
                             std::to_string(format));
  }
}

inline auto AlignUp(size_t offset, size_t alignment) -> size_t {
  return (offset + alignment - 1) & ~(alignment - 1);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
// TODO: Refactor this function.
// We need to calculate the stride of a buffer format
// And we also need write a final offset relative to the place of that format in the parent format for each component
auto BufferFormat::CalculateStride(Standard std, size_t &offset) -> void {
  if (Components.empty()) {
    PrintWarning("Buffer format has no components, stride will be 0");
    return;
  }

  size_t maxAlignment = 0;
  for (auto &format : Components) {
    size_t baseAlign = 0;

    // Get base alignment of component
    if (std::holds_alternative<VkFormat>(format.format)) {
      auto vulkanFormat = std::get<VkFormat>(format.format);
      VkFormat alignFormat =
          format.arraySize == 1
              ? vulkanFormat
              : Graphics::Format::GetVec4Variant(vulkanFormat);
      auto alignResult = GetAlignment(alignFormat);
      if (Error::IsError(alignResult)) {
        return;
      }

      baseAlign = alignResult.value();
    } else if (std::holds_alternative<BufferFormat>(format.format)) {
      auto bufferFormat = std::get<BufferFormat>(format.format);
      bufferFormat.CalculateStride(std, offset);

      baseAlign = bufferFormat.alignment;
    }

    size_t compAlignment = 0;

    // Get allowed alignment based on standard
    switch (std) {
    case Standard::Std140:
      compAlignment = (format.arraySize == 1)
                          ? baseAlign
                          : std::max(baseAlign, 16UL); // NOLINT
      break;
    case Standard::Std430:
      compAlignment = baseAlign;
      break;
    default:
      assert(false && "Something went very wrong");
      break;
    }
    maxAlignment = std::max(maxAlignment, compAlignment);
    size_t size = 0;

    // Get size of component
    if (std::holds_alternative<VkFormat>(format.format)) {
      auto vulkanFormat = std::get<VkFormat>(format.format);
      size = Graphics::Format::GetSize(vulkanFormat);
    } else if (std::holds_alternative<BufferFormat>(format.format)) {
      auto bufferFormat = std::get<BufferFormat>(format.format);
      PrintAlways("Offset before calculating nested format: {}", offset);
      bufferFormat.CalculateStride(std, offset);
      PrintAlways("Offset after calculating nested format: {}", offset);

      size = bufferFormat.stride;
    }

    // Align component
    offset = AlignUp(offset, compAlignment);
    format.offset = offset;

    // Add padding
    if (format.arraySize == 1) {
      offset += size;
    } else {
      switch (std) {
      case Standard::Std140:
        offset += AlignUp(size, 16) * format.arraySize; // NOLINT
        break;
      case Standard::Std430:
        offset += AlignUp(size, compAlignment) * format.arraySize;
        break;
      default:
        assert(false && "Something went very wrong");
        break;
      }
    }
  }

  // Pad final structure
  switch (std) {
  case Standard::Std140: {
    size_t finalAlignment = std::max(maxAlignment, 16UL); // NOLINT
    alignment = finalAlignment;
    stride = AlignUp(offset, finalAlignment);
    break;
  }
  case Standard::Std430: {
    alignment = maxAlignment;
    stride = AlignUp(offset, maxAlignment);
    break;
  }
  }

  initialized = true;
}

auto BufferFormat::FlattenComponentTree() -> void {
  for (auto &component : Components) {
    if (std::holds_alternative<VkFormat>(component.format)) {
      auto &bufferComp = std::get<VkFormat>(component.format);
      // Copy this component for each array element
      for (int i = 0; i < component.arraySize; i++) {
        FlatComponents.emplace_back(bufferComp);
      }
    } else if (std::holds_alternative<BufferFormat>(component.format)) {
      auto &format = std::get<BufferFormat>(component.format);

      if (format.FlatComponents.empty()) {
        format.FlattenComponentTree();
      }

      for (auto &comp : format.FlatComponents) {
        FlatComponents.emplace_back(comp);
      }
    }
  }
}

[[nodiscard]] auto BufferFormat::FormatAt(size_t componentOffset) -> VkFormat {
  if (FlatComponents.empty()) {
    FlattenComponentTree();
  }

  componentOffset %= FlatComponents.size();

  return FlatComponents[componentOffset];
}

[[nodiscard]] auto BufferFormat::ToString(int indentation) const
    -> std::string {
  auto baseTabs = std::string(indentation * 2UL, ' ');
  std::ostringstream output;
  output << baseTabs << "{\n";
  indentation++;

  for (const auto &component : Components) {
    auto tabs = std::string(indentation * 2UL, ' ');
    auto arrayStr = component.arraySize == 1
                        ? ""
                        : std::format("[{}]", component.arraySize);
    if (std::holds_alternative<VkFormat>(component.format)) {
      auto format = std::get<VkFormat>(component.format);
      auto formatname = Graphics::Format::ToString(format);
      output << std::format("{}{} {}{};\n", tabs, component.name, formatname,
                            arrayStr);
    } else {
      auto format = std::get<Graphics::BufferFormat>(component.format);
      output << std::format("{}{}\n{}{}", tabs, component.name,
                            format.ToString(indentation), arrayStr);
    }
  }

  output << baseTabs << "};\n";

  return output.str();
}

} // namespace Graphics