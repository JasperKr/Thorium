#include "push.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include "tl/expected.hpp"
#include <vector>

namespace Graphics {
auto PushBuffer::FlushData(FlushInfo &info) -> void {
  auto bufferSize = layout.size;

  if (layout.IsStruct()) {
    bufferSize = std::get<StructInfo>(layout.info).size;
  }

  vkCmdPushConstants(info.commandBuffer, info.pipelineLayout, stageFlags,
                     layout.offset, bufferSize, data.data());
}
auto PushBuffer::InfoOf(const std::string &name) const
    -> tl::expected<ResourceInfo, Error::Error> {
  if (!layout.IsStruct()) {
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