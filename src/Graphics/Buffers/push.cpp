#include "push.hpp"
#include "Modules/error.hpp"
#include "tl/expected.hpp"
#include <vector>

namespace Graphics {
auto PushBuffer::FlushData(FlushInfo &info) -> void {
  vkCmdPushConstants(info.commandBuffer, info.pipelineLayout, info.stageFlags,
                     layout.offset, layout.size, data.data());
}
auto PushBuffer::OffsetOf(const std::string &name) const
    -> tl::expected<size_t, Error::Error> {
  if (layout.type != BufferResourceType::Struct) {
    return Error::Unexpected("OffsetOf only supported for struct push buffers");
  }

  const auto &structInfo = std::get<StructInfo>(layout.info);
  for (const auto &field : structInfo.fields) {
    if (field.name == name) {
      switch (field.variant) {
      case StructFieldVariant::Scalar: {
        auto info = std::get<ScalarInfo>(field.info);
        return info.offset;
      }
      case StructFieldVariant::Vector: {
        auto info = std::get<VectorInfo>(field.info);
        return info.offset;
      }
      case StructFieldVariant::Matrix: {
        auto info = std::get<MatrixInfo>(field.info);
        return info.offset;
      }
      default:
        return Error::Unexpected("Unsupported field variant in struct");
      }
    }
  }

  return Error::Unexpected("Field name not found in struct");
}
} // namespace Graphics