#include "push.hpp"
#include "Graphics/reflect.hpp"
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace Graphics {

PushBuffer::PushBuffer(Reflect::FlattenedReflection reflection,
                       VkShaderStageFlags stage)
    : layout(std::move(reflection)), stageFlags(stage) {

  data.resize(layout.size);
}

auto PushBuffer::GetBufferSize() const -> size_t { return layout.size; }

auto PushBuffer::GetLayout() const -> const Reflect::FlattenedReflection & {
  return layout;
}

auto PushBuffer::FlushData(FlushInfo &info) -> void {
  auto bufferSize = GetBufferSize();

  vkCmdPushConstants(info.commandBuffer, info.pipelineLayout, stageFlags,
                     GetBufferOffset(), bufferSize, data.data());
}

auto PushBuffer::ContainsUniform(const ResourceKey &key) const -> bool {
  return layout.keyToInfo.contains(key);
}

auto PushBuffer::GetUniformOffset(const ResourceKey &key) const
    -> std::optional<uint32_t> {
  auto iter = layout.keyToInfo.find(key);
  if (iter == layout.keyToInfo.end()) {
    return std::nullopt;
  }

  return iter->second.offset;
}

auto PushBuffer::GetUniform(const ResourceKey &key) const
    -> const Reflect::ResourceInfo * {
  auto iter = layout.keyToInfo.find(key);
  if (iter == layout.keyToInfo.end()) {
    return nullptr;
  }

  return &iter->second;
}

auto PushBuffer::GetStageFlags() const -> VkShaderStageFlags {
  return stageFlags;
}

// NOLINTNEXTLINE
auto PushBuffer::GetBufferOffset() const -> size_t {
  return 0; // TODO
}

auto PushBuffer::SetData(const ResourceKey &key,
                         const std::span<const uint8_t> &values) -> Error {

  auto offsetOpt = GetUniformOffset(key);
  if (!offsetOpt.has_value()) {
    return Error::Create("Uniform `" + Reflect::ResourceKeyToString(key) +
                         "` not found in push buffer.");
  }

  auto offset = offsetOpt.value();

  if (offset + values.size() > data.size()) {
    return Error::Create("Data exceeds buffer size");
  }

  // NOLINTNEXTLINE
  std::memcpy(data.data() + offset, values.data(), values.size());

  return Error::Success();
}

} // namespace Graphics