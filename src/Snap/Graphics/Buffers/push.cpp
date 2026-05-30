#include "push.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/console.hpp"
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

namespace Graphics {

PushBuffer::PushBuffer(Reflect::ResourceInfo inputLayout,
                       VkShaderStageFlags stage)
    : layout(std::move(inputLayout)), stageFlags(stage) {

  if (!layout.IsBuffer()) {
    PrintError("PushBuffer layout must be a buffer.");
    return;
  }

  auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

  if (std::holds_alternative<Reflect::ScalarInfo>(bufferInfo.info)) {
    data.resize(std::get<Reflect::ScalarInfo>(bufferInfo.info).size);
  } else if (std::holds_alternative<Reflect::VectorInfo>(bufferInfo.info)) {
    data.resize(std::get<Reflect::VectorInfo>(bufferInfo.info).size);
  } else if (std::holds_alternative<Reflect::MatrixInfo>(bufferInfo.info)) {
    data.resize(std::get<Reflect::MatrixInfo>(bufferInfo.info).size);
  } else if (std::holds_alternative<Reflect::StructInfo>(bufferInfo.info)) {
    data.resize(std::get<Reflect::StructInfo>(bufferInfo.info).size);
  }

  bufferInfo.name = layout.name;
  bufferInfo.GetInfo<Reflect::StructInfo>().name = layout.name;
}

auto PushBuffer::GetBufferSize() const -> size_t {
  const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);
  if (std::holds_alternative<Reflect::StructInfo>(bufferInfo.info)) {
    return std::get<Reflect::StructInfo>(bufferInfo.info).size;
  }
  return bufferInfo.size;
}

auto PushBuffer::GetLayout() const -> const Reflect::ResourceInfo & {
  return layout;
}

auto PushBuffer::FlushData(FlushInfo &info) -> void {
  const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);
  auto bufferSize = bufferInfo.size;

  if (bufferInfo.IsStruct()) {
    bufferSize = std::get<Reflect::StructInfo>(bufferInfo.info).size;
  }

  vkCmdPushConstants(info.commandBuffer, info.pipelineLayout, stageFlags,
                     bufferInfo.offset, bufferSize, data.data());
}

auto PushBuffer::ContainsUniform(ResourceKey::const_iterator iterator,
                                 ResourceKey::const_iterator end) const
    -> bool {

  if (std::next(iterator) == end) {
    // return *iterator == layout.name;
    return strcmp(*iterator, layout.name) == 0;
  }

  if (strcmp(*iterator, layout.name) != 0) {
    return false;
  }

  const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

  return bufferInfo.ResolvePath(iterator, end) != nullptr;
}

auto PushBuffer::GetUniform(ResourceKey::const_iterator iterator,
                            ResourceKey::const_iterator end) const
    -> const Reflect::ResourceInfo * {
  if (std::next(iterator) == end) {
    // if (*iterator == layout.name) {
    if (strcmp(*iterator, layout.name) == 0) {
      return &layout;
    }

    return nullptr;
  }

  const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

  return bufferInfo.ResolvePath(iterator, end);
}

auto PushBuffer::GetStageFlags() const -> VkShaderStageFlags {
  return stageFlags;
}

auto PushBuffer::GetBufferOffset() const -> size_t {
  // const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info); // TODO: Use this?
  return layout.offset;
}

auto PushBuffer::SetData(const ResourceKey &key,
                         const std::span<const uint8_t> &values) -> Error {

  const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

  if (!std::holds_alternative<Reflect::StructInfo>(bufferInfo.info)) {
    return Error::Create(
        "SetData with key only supported for struct push buffers");
  }

  const auto *result = GetUniform(key.begin(), key.end());
  if (result == nullptr) {
    return Error::Create("Uniform not found in push buffer.");
  }

  size_t offset = result->GetOffset();

  if (result->GetSize() != values.size()) {
    return Error::Create("Data size does not match field size");
  }

  if (offset + values.size() > data.size()) {
    return Error::Create("Data exceeds buffer size");
  }

  // NOLINTNEXTLINE
  std::memcpy(data.data() + offset, values.data(), values.size());

  return Error::Success();
}

auto PushBuffer::SetData(const std::span<const uint8_t> &values) -> Error {
  const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

  if (!bufferInfo.IsScalar() && !bufferInfo.IsVector() &&
      !bufferInfo.IsMatrix()) {
    return Error::Create("SetData without name only supported for scalar, "
                         "vector, and matrix push buffers");
  }

  if (values.size() > data.size()) {
    return Error::Create("Data exceeds buffer size");
  }

  std::memcpy(data.data(), values.data(), values.size());

  return Error::Success();
}

} // namespace Graphics