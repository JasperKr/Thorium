#include "bufferformat.hpp"
#include "Graphics/hash.hpp"

namespace Graphics {
BufferFormat::BufferFormat(std::vector<BufferComponent> components)
    : Components(std::move(components)) {
  // calculate offsets
  size_t currentOffset = 0;
  size_t totalChannels = 0;
  for (auto &component : Components) {
    component.offset = static_cast<uint32_t>(currentOffset);
    currentOffset += Graphics::Format::GetSize(component.format);
    totalChannels += Graphics::Format::GetChannelCount(component.format);
  }
  FormatsAtOffsets.resize(totalChannels, VK_FORMAT_UNDEFINED);

  auto index = 0;
  for (const auto &component : Components) {
    size_t formatSize = Graphics::Format::GetSize(component.format);
    size_t channelCount = Graphics::Format::GetChannelCount(component.format);

    for (size_t i = 0; i < channelCount; i++) {
      FormatsAtOffsets[index++] = component.format;
    }
  }

  assert(index == FormatsAtOffsets.size());
}

auto BufferFormat::GetSize() const -> size_t {
  size_t size = 0;
  for (const auto &component : Components) {
    size += Graphics::Format::GetSize(component.format);
  }
  return size;
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

} // namespace Graphics