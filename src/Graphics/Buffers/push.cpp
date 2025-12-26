#include "push.hpp"
#include "Graphics/reflect.hpp"
#include <vector>

namespace Graphics {
auto PushBuffer::FlushData(FlushInfo &info) -> void {
  auto &bufferInfo = std::get<BufferInfo>(layout.info);
  auto bufferSize = bufferInfo.size;

  if (bufferInfo.IsStruct()) {
    bufferSize = std::get<StructInfo>(bufferInfo.info).size;
  }

  vkCmdPushConstants(info.commandBuffer, info.pipelineLayout, stageFlags,
                     bufferInfo.offset, bufferSize, data.data());
}

auto PushBuffer::ContainsUniform(ResourceKey::const_iterator iterator,
                                 ResourceKey::const_iterator end) const
    -> bool {
  // TODO: Probably broken
  if (std::next(iterator) == end) {
    return *iterator == layout.name;
  }

  const auto &bufferInfo = std::get<BufferInfo>(layout.info);

  return bufferInfo.ResolvePath(std::next(iterator), end) != nullptr;
}

auto PushBuffer::GetUniform(ResourceKey::const_iterator iterator,
                            ResourceKey::const_iterator end) const
    -> const ResourceInfo * {
  // TODO: Probably broken
  if (std::next(iterator) == end) {
    if (*iterator == layout.name) {
      return &layout;
    }

    return nullptr;
  }

  const auto &bufferInfo = std::get<BufferInfo>(layout.info);

  return bufferInfo.ResolvePath(std::next(iterator), end);
}
} // namespace Graphics