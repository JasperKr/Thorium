#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <cstdint>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics::Threading {

struct RenderThreadData {
  // The usage updates recorded for all worker threads
  // We will append resource barriers after each command buffer recording
  // After reordering at the end of the frame
  std::vector<std::pair<Barrier::BarrierSynced, Barrier::ResourceState>>
      usageUpdates;
  std::vector<Barrier::ResourceSync> resourceSyncs;

  uint64_t key = 0;
  int64_t priority = 0; // Tie-breaker for overlapping keys

  VkCommandBuffer commandBuffer = nullptr;
  bool drawsToSwapchain = false;
  uint64_t aquiredAtFrame = 0;

  std::string name;
  uint64_t id;
};

const Type renderInfoType = Type("Internal RenderThreadInfo");

struct RenderThreadInfo : Object {
  RenderThreadData threadData;

  auto GetInstanceType() const -> Type const * override {
    return &renderInfoType;
  }

  static auto GetType() -> Type const * { return &renderInfoType; }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern std::mutex ResultsMutex;
extern std::vector<Ref<RenderThreadInfo>> Results;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local Ref<RenderThreadInfo> CurrentRenderThreadInfo;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

struct AquireInfo {
  std::string name;
  int64_t priority;
};

// Aquire a command buffer for the current thread, must have rendering permission
auto AquireCommandBuffer(Graphics::GraphicsContext &context,
                         const AquireInfo &info)
    -> Result<Ref<RenderThreadInfo>>;

// Submit the commands recorded on the current thread
auto SubmitCommands(Graphics::GraphicsContext &context) -> Error;

// Initialize the render threading module
auto Initialize(Graphics::GraphicsContext &context) -> Error;

// Deinitialize the render threading module
auto Deinitialize(Graphics::GraphicsContext &context) -> Error;

// Get all generated command names
auto GetGeneratedCommands() -> std::vector<std::string>;

} // namespace Graphics::Threading