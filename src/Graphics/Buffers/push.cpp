#include "push.hpp"
#include "Modules/error.hpp"
#include "tl/expected.hpp"
#include <vector>

namespace Graphics {
auto PushBuffer::FlushData(FlushInfo &info) -> void {
  auto bufferSize = layout.size;

  if (layout.type == BufferResourceType::Struct) {
    bufferSize = std::get<StructInfo>(layout.info).size;
  }

  vkCmdPushConstants(info.commandBuffer, info.pipelineLayout, stageFlags,
                     layout.offset, bufferSize, data.data());
}
auto PushBuffer::InfoOf(const std::string &name) const
    -> tl::expected<StructFieldInfo, Error::Error> {
  if (layout.type != BufferResourceType::Struct) {
    return Error::Unexpected("OffsetOf only supported for struct push buffers");
  }

  const auto &structInfo = std::get<StructInfo>(layout.info);
  for (const auto &field : structInfo.fields) {
    if (field.name == name) {
      return field;
    }
  }

  return Error::Unexpected("Field name not found in struct");
}
} // namespace Graphics