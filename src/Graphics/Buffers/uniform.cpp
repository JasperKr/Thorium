#include "uniform.hpp"
#include "Graphics/graphics.hpp"
#include <cassert>
#include <vector>

namespace Graphics {

thread_local std::vector<FrameUniformBufferObject>
    ThreadUniformBuffers{}; // NOLINT

auto InitializeUniformBufferModule(GraphicsContext &context) -> Error {
  for (uint32_t j = 0; j < FRAMES_IN_FLIGHT; j++) {
    auto createResult = FrameUniformBufferObject::Create(context);

    if (Error::IsError(createResult)) {
      return createResult.error();
    }

    ThreadUniformBuffers.emplace_back(createResult.value());
  }

  return Error::Success();
}

auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void {
  ThreadUniformBuffers.clear();
}

auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject & {
  assert(frameIndex < ThreadUniformBuffers.size() &&
         "Frame index out of bounds in GetGlobalUniformBuffer");
  return ThreadUniformBuffers.at(frameIndex);
}

} // namespace Graphics