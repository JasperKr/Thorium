#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <sys/types.h>
#include <vector>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace Graphics::Threading {

struct RenderThreadData {
  // The usage updates recorded for all worker threads
  // We will append resource barriers after each command buffer recording
  // After reordering at the end of the frame
  std::vector<Barrier::ResourceState> usageUpdates;
  std::vector<Barrier::ResourceSync> resourceSyncs;

  uint64_t index;
  uint64_t queueID;

  VkCommandBuffer commandBuffer = nullptr;
};

struct RenderThreadInfo {
  RenderThreadData threadData;

  // Protects the `currentlyRecording` flag
  std::mutex availabilityMutex;
  std::condition_variable availabilityCV;
  bool currentlyRecording = false;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern std::mutex CanStartNewCommandsMutex;
extern bool CanStartNewCommands;
extern std::condition_variable CanStartNewCommandsCV;
extern std::mutex ResultsMutex;
extern std::vector<Ref<RenderThreadInfo>> Results;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto HasRenderingPermission() -> bool;
auto DemandRenderingPermission() -> void;

struct AquireInfo {

  // used later on the main thread for reordering command buffers
  // this will be sorted to be inorder, does NOT have to be continuous,
  // you MAY give the same index to multiple command buffers for unordered execution
  uint32_t commandIndex = UINT64_MAX;

  // used to identify which queue this command buffer will be submitted to
  // useful for asyncing multiple queues
  uint32_t queueID = 0;
};

// Aquire a command buffer for the current thread, must have rendering permission

auto AquireCommandBuffer(Graphics::GraphicsContext &context, AquireInfo info)
    -> Result<Ref<RenderThreadInfo>>;
auto SubmitCommands(Graphics::GraphicsContext &context,
                    RenderThreadInfo &threadInfo) -> Error;

} // namespace Graphics::Threading