#include "uniform.hpp"
#include <cassert>
#include <vector>

namespace Graphics {

// NOLINTNEXTLINE
thread_local std::vector<FrameUniformBufferObject> ThreadUniformBuffers{};

auto InitializeUniformBufferModule(GraphicsContext &context) -> Error::Error {
  for (uint32_t i = 0; i < context.renderThreadCount; i++) {
    ThreadUniformBuffers.emplace_back(context);
  }

  return Error::Success();
}

auto GetGlobalUniformBuffer() -> FrameUniformBufferObject & {
  thread_local auto threadIndex = GetCurrentThreadIndex();
  assert(threadIndex < ThreadUniformBuffers.size());
  return ThreadUniformBuffers.at(threadIndex);
}

} // namespace Graphics