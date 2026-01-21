#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace Graphics::Threading {

struct RenderThreadData {
  // The usage updates recorded for all worker threads
  // We will append resource barriers after each command buffer recording
  // After reordering at the end of the frame
  std::vector<std::pair<Barrier::GraphicsResource, Barrier::ResourceState>>
      usageUpdates;
  std::vector<Barrier::ResourceSync> resourceSyncs;

  uint64_t key = 0;
  int64_t priority = 0; // Tie-breaker for overlapping keys

  VkCommandBuffer commandBuffer = nullptr;

#ifndef NDEBUG // If debug mode is not not defined..
  std::string name;
#endif
};

const Type renderInfoType = Type("Internal RenderThreadInfo");

struct RenderThreadInfo : Object {
  RenderThreadData threadData;

  // Protects the `currentlyRecording` flag
  std::mutex availabilityMutex;
  std::condition_variable availabilityCV;
  bool currentlyRecording = false;

  auto GetInstanceType() const -> Type const * override {
    return &renderInfoType;
  }

  static auto GetType() -> Type const * { return &renderInfoType; }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern std::mutex CanStartNewCommandsMutex;
extern bool CanStartNewCommands;
extern std::condition_variable CanStartNewCommandsCV;
extern std::mutex ResultsMutex;
extern std::vector<Ref<RenderThreadInfo>> Results;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local Ref<RenderThreadInfo> CurrentRenderThreadInfo;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto HasRenderingPermission() -> bool;
auto DemandRenderingPermission() -> void;

struct AquireInfo {
  std::string name;
  int64_t priority;
};

// Aquire a command buffer for the current thread, must have rendering permission

auto AquireCommandBuffer(Graphics::GraphicsContext &context,
                         const AquireInfo &info)
    -> Result<Ref<RenderThreadInfo>>;
auto SubmitCommands(Graphics::GraphicsContext &context) -> Error;
auto Initialize(Graphics::GraphicsContext &context) -> Error;
auto Deinitialize(Graphics::GraphicsContext &context) -> void;

} // namespace Graphics::Threading