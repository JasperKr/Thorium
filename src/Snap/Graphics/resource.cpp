#include "resource.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

std::vector<GraphicsMemory> ReleasedGraphicsMemory{};

std::mutex ReleasedGraphicsMemoryMutex{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

template <typename T>
  requires(std::is_invocable_v<decltype(&T::Destroy), const GraphicsContext &,
                               void *>)
inline auto Schedule(const T &data, uint64_t timelineValue) -> void {
  std::lock_guard<std::mutex> lock(ReleasedGraphicsMemoryMutex);

  // NOLINTNEXTLINE
  auto memory = std::make_unique<std::byte[]>(sizeof(T));
  new (memory.get()) T(data);

  ReleasedGraphicsMemory.emplace_back(GraphicsMemory{
      .timelineValue = timelineValue,
      .destroy = T::Destroy,
      .resourceHandle = std::move(memory),
  });
}

auto ScheduleDestruction(const TextureMemory &texture, uint64_t timelineValue)
    -> void {
  Schedule(texture, timelineValue);
}

auto ScheduleDestruction(const BufferMemory &buffer, uint64_t timelineValue)
    -> void {
  Schedule(buffer, timelineValue);
}

auto ScheduleDestruction(const TextureViewMemory &textureView,
                         uint64_t timelineValue) -> void {
  Schedule(textureView, timelineValue);
}

auto ScheduleDestruction(const ShaderModuleMemory &shaderModule,
                         uint64_t timelineValue) -> void {
  Schedule(shaderModule, timelineValue);
}

auto ScheduleDestruction(const PipelineMemory &pipeline, uint64_t timelineValue)
    -> void {
  Schedule(pipeline, timelineValue);
}

auto ScheduleDestruction(const AccelerationStructureMemory &resource,
                         uint64_t timelineValue) -> void {
  Schedule(resource, timelineValue);
}

auto TextureMemory::Destroy(const GraphicsContext &context, void *data)
    -> void {

  auto *textureMemory = reinterpret_cast<TextureMemory *>(data);

  vmaDestroyImage(context.vmaAllocator, textureMemory->image,
                  textureMemory->allocation);
}

auto BufferMemory::Destroy(const GraphicsContext &context, void *data) -> void {
  auto *bufferMemory = reinterpret_cast<BufferMemory *>(data);

  vmaDestroyBuffer(context.vmaAllocator, bufferMemory->buffer,
                   bufferMemory->allocation);
}

auto TextureViewMemory::Destroy(const GraphicsContext &context, void *data)
    -> void {
  auto *textureViewMemory = reinterpret_cast<TextureViewMemory *>(data);

  vkDestroyImageView(context.device, textureViewMemory->imageView,
                     GetAllocationCallbacks());
}

auto ShaderModuleMemory::Destroy(const GraphicsContext &context, void *data)
    -> void {
  auto *shaderModuleMemory = reinterpret_cast<ShaderModuleMemory *>(data);

  vkDestroyShaderModule(context.device, shaderModuleMemory->shaderModule,
                        GetAllocationCallbacks());
}

auto PipelineMemory::Destroy(const GraphicsContext &context, void *data)
    -> void {
  auto *pipelineMemory = reinterpret_cast<PipelineMemory *>(data);

  vkDestroyPipeline(context.device, pipelineMemory->pipeline,
                    GetAllocationCallbacks());
}

auto AccelerationStructureMemory::Destroy(const GraphicsContext &context,
                                          void *data) -> void {
  auto *accelStructMemory =
      reinterpret_cast<AccelerationStructureMemory *>(data);

  vkDestroyAccelerationStructureKHR(context.device,
                                    accelStructMemory->accelerationStructure,
                                    GetAllocationCallbacks());
}

inline auto CanBeDestroyed( // NOLINTNEXTLINE
    const uint64_t &resourceTimelineValue) -> bool {

  return !Graphics::semaphoreManager.IsInUse(resourceTimelineValue);
}

auto ProcessReleasedResources(GraphicsContext &context) -> void {
  if (!GetDeferredDestructionAllowed()) {
    std::scoped_lock<std::mutex, std::mutex, std::mutex> lock(
        ReleasedGraphicsMemoryMutex, Graphics::GraphicsContext::mutexes.device,
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    for (auto &res : ReleasedGraphicsMemory) {
      res.destroy(context, res.resourceHandle.get());
    }

    return;
  }

  {
    std::scoped_lock<std::mutex, std::mutex> lock(
        Graphics::GraphicsContext::mutexes.device,
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    {
      std::lock_guard<std::mutex> lock(ReleasedGraphicsMemoryMutex);

      Utils::UnorderedErase(ReleasedGraphicsMemory,
                            [&](GraphicsMemory &res) -> auto {
                              if (CanBeDestroyed(res.timelineValue)) {
                                res.destroy(context, res.resourceHandle.get());
                                return true;
                              }
                              return false;
                            });
    }
  }
}

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace Graphics