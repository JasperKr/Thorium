#include "push.hpp"
#include <vector>

namespace Graphics {
auto PushBuffer::FlushData(FlushInfo &info) -> void {
  vkCmdPushConstants(info.commandBuffer, info.pipelineLayout, info.stageFlags,
                     layout.offset, layout.size, data.data());
}
auto PushBuffer::OffsetOf(const std::string &name) const -> size_t {}
} // namespace Graphics