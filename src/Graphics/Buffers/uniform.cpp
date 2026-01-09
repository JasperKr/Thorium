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
      auto createResult = FrameUniformBufferObject::Create(context);

      if (Error::IsError(createResult)) {
        return createResult.error();
      }

      ThreadUniformBuffers[i].emplace_back(createResult.value());
    }
  }

  return Error::Success();
}

auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void {
  for (auto &threadBuffers : ThreadUniformBuffers) {
    for (auto &bufferObj : threadBuffers) {
      bufferObj.Destroy(context);
    }
    threadBuffers.clear();
  }
  ThreadUniformBuffers.clear();
}

auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject & {
  thread_local auto threadIndex = GetCurrentThreadIndex();
  assert(threadIndex < ThreadUniformBuffers.size());
  assert(frameIndex < FRAMES_IN_FLIGHT);
  return ThreadUniformBuffers.at(threadIndex).at(frameIndex);
}

} // namespace Graphics