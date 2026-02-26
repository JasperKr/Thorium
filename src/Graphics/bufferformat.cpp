#include "bufferformat.hpp"
#include "Graphics/format.hpp"
#include "Graphics/hash.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace Graphics {
BufferFormat::BufferFormat(std::vector<BufferComponent> components)
    : Components(std::move(components)) {
  // calculate offsets

  auto strideResult = ::Graphics::GetElementStride(Components, Standard::Std430,
                                                   ComponentOffsets);
  if (Error::IsError(strideResult)) {
    PrintError("Error calculating buffer format stride: %s",
               strideResult.error().message.c_str());
    stride = 0;
    return;
  }
  stride = strideResult.value();

  size_t totalIndices = 0;
  for (const auto &component : Components) {
    size_t channelCount = Graphics::Format::GetChannelCount(component.format);
    totalIndices += channelCount * component.arraySize;
  }

  FormatsAtIndices.resize(totalIndices, VK_FORMAT_UNDEFINED);

  auto index = 0;
  for (const auto &component : Components) {
    size_t channelCount = Graphics::Format::GetChannelCount(component.format);

    for (size_t i = 0; i < channelCount; i++) {
      for (size_t arrayIndex = 0; arrayIndex < component.arraySize;
           arrayIndex++) {
        FormatsAtIndices[index++] = component.format;
      }
    }
  }

  assert(index == FormatsAtIndices.size());
}

auto BufferFormat::GetElementStride() const -> size_t { return stride; }
auto BufferFormat::GetComponentOffset(size_t componentIndex) const -> size_t {
  if (componentIndex >= Components.size()) {
    return 0;
  }

  return ComponentOffsets[componentIndex];
}
auto BufferFormat::GetComponentOffset(const ResourceKey &name) const
    -> Result<size_t> {
  for (size_t i = 0; i < Components.size(); ++i) {
    if (Components[i].name == name) {
      return ComponentOffsets[i];
    }
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
      hasher.add(std::hash<std::string>()(*it));
    }

    hasher.add(std::hash<uint32_t>()(attribute.format));
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

auto GetAlignment(const BufferComponent &component) -> Result<size_t> {
  return GetAlignment(component.format);
}

inline auto AlignUp(size_t offset, size_t alignment) -> size_t {
  return (offset + alignment - 1) & ~(alignment - 1);
}

// Get the stride of an element with the given formats according to the specified standard, and fill offsets with the byte offset of each format within the element
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto GetElementStride(const std::vector<BufferComponent> &formats, Standard std,
                      std::vector<size_t> &offsets) -> Result<size_t> {
  if (formats.empty()) {
    return Error::Unexpected(
        "Format list cannot be empty for alignment calculation.");
  }

  switch (std) {
  case Standard::Std140: {
    size_t offset = 0;
    size_t maxAlignment = 0;
    for (const auto &format : formats) {
      // Arrays in std140 use vec4 alignment
      VkFormat alignFormat =
          format.arraySize == 1
              ? format.format
              : Graphics::Format::GetVec4Variant(format.format);
      auto alignResult = GetAlignment(alignFormat);
      if (Error::IsError(alignResult)) {
        offsets.clear();
        return alignResult.error().AsUnexpected();
      }

      size_t baseAlign = alignResult.value();
      size_t alignment = (format.arraySize == 1)
                             ? baseAlign
                             : std::max(baseAlign, 16UL); // NOLINT
      maxAlignment = std::max(maxAlignment, alignment);
      size_t size = Graphics::Format::GetSize(format.format);
      offset = AlignUp(offset, alignment);
      offsets.emplace_back(offset);
      if (format.arraySize == 1) {
        offset += size;
      } else {
        offset += AlignUp(size, 16) * format.arraySize; // NOLINT
      }
    }

    size_t finalAlignment = std::max(maxAlignment, 16UL); // NOLINT
    return AlignUp(offset, finalAlignment);
  }
  case Standard::Std430: {
    size_t offset = 0;
    size_t maxAlignment = 0;
    for (const auto &format : formats) {
      auto alignResult = GetAlignment(format.format);
      if (Error::IsError(alignResult)) {
        offsets.clear();
        return alignResult.error().AsUnexpected();
      }

      size_t alignment = alignResult.value();
      maxAlignment = std::max(maxAlignment, alignment);
      size_t size = Graphics::Format::GetSize(format.format);
      offset = AlignUp(offset, alignment);
      offsets.emplace_back(offset);
      if (format.arraySize == 1) {
        offset += size;
      } else {
        offset += AlignUp(size, alignment) * format.arraySize;
      }
    }

    return AlignUp(offset, maxAlignment);
  }
  default:
    return Error::Unexpected(
        "Unsupported standard for buffer format alignment: " +
        std::to_string(static_cast<uint32_t>(std)));
  }
}

} // namespace Graphics