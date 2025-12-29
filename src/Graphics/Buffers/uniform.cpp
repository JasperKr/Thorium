#include "uniform.hpp"
#include "Graphics/graphics.hpp"
#include <cassert>
#include <vector>

namespace Graphics {

thread_local std::vector<std::vector<FrameUniformBufferObject>>
    ThreadUniformBuffers{}; // NOLINT

auto InitializeUniformBufferModule(GraphicsContext &context) -> Error {
  for (uint32_t i = 0; i < context.renderThreadCount; i++) {
    ThreadUniformBuffers.emplace_back();
    for (uint32_t j = 0; j < FRAMES_IN_FLIGHT; j++) {
      ThreadUniformBuffers[i].emplace_back(context);
    }
  }

  return Error::Success();
}

auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject & {
  thread_local auto threadIndex = GetCurrentThreadIndex();
  assert(threadIndex < ThreadUniformBuffers.size());
  assert(frameIndex < FRAMES_IN_FLIGHT);
  return ThreadUniformBuffers.at(threadIndex).at(frameIndex);
}

} // namespace Graphics