#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/sampler.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics::Threading {

struct RenderThreadData {
  // The usage updates recorded for all worker threads
  // We will append resource barriers after each command buffer recording
  // After reordering at the end of the frame
  std::vector<std::pair<Barrier::AccessState, Barrier::ResourceState>>
      usageUpdates;
  std::vector<Barrier::ResourceSync> resourceSyncs;

  uint64_t key = 0;
  int64_t priority = 0; // Tie-breaker for overlapping keys

  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  uint64_t cmdBufferTimelineValue{};
  bool drawsToSwapchain = false;
  uint64_t acquiredAtFrame{};

  std::vector<std::pair<std::weak_ptr<ImageMemory>, ImageState>>
      initialImageStates;

  std::unordered_map<ObjectID, ImageState> finalImageStates;

  std::string name;
  uint64_t id{};
};

const Type renderInfoType = Type("Commands");

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern std::mutex CommandBufferCacheMutex;
extern std::vector<std::pair<uint64_t, VkCommandBuffer>> CommandBufferCache;

struct RenderThreadInfo : Object {
  RenderThreadInfo(const RenderThreadInfo &) = delete;
  RenderThreadInfo(RenderThreadInfo &&) = delete;
  auto operator=(const RenderThreadInfo &) -> RenderThreadInfo & = delete;
  auto operator=(RenderThreadInfo &&) -> RenderThreadInfo & = delete;
  explicit RenderThreadInfo() : threadData() {}
  RenderThreadData threadData;

  static auto GetType() -> Type const * { return &renderInfoType; }
  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return &renderInfoType;
  }

  ~RenderThreadInfo() override {
    if (threadData.commandBuffer != VK_NULL_HANDLE) {
      std::lock_guard<std::mutex> lock(CommandBufferCacheMutex);
      CommandBufferCache.emplace_back(threadData.cmdBufferTimelineValue,
                                      threadData.commandBuffer);
      threadData.commandBuffer = VK_NULL_HANDLE;
    }
  }
};

extern thread_local Ref<RenderThreadInfo> CurrentRenderThreadInfo;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

struct AcquireInfo {
  std::string name;
  int64_t priority;
};

// Acquire a command buffer for the current thread, must have rendering permission
auto AcquireCommandBuffer(Graphics::GraphicsContext &context,
                          const AcquireInfo &info)
    -> Result<Ref<RenderThreadInfo>>;

// Submit the commands recorded on the current thread
auto SubmitCommands(Graphics::GraphicsContext &context)
    -> Result<Ref<RenderThreadInfo>>;

// Initialize the render threading module
auto Initialize(Graphics::GraphicsContext &context) -> Error;

// Deinitialize the render threading module
auto Deinitialize(Graphics::GraphicsContext &context) -> Error;

struct GraphicsConfiguration {
  SamplerDescription defaultSamplerDescription{};
};

auto GetGraphicsConfiguration() -> GraphicsConfiguration &;

} // namespace Graphics::Threading