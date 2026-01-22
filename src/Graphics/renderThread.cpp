#include "Graphics/renderThread.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/shader.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cassert>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Graphics::Threading {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

std::mutex CanStartNewCommandsMutex{};
bool CanStartNewCommands{};
std::condition_variable CanStartNewCommandsCV{};
std::mutex ResultsMutex{};
std::vector<Ref<RenderThreadInfo>> Results{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto HasRenderingPermission() -> bool {
  std::lock_guard<std::mutex> lock(CanStartNewCommandsMutex);
  return CanStartNewCommands;
}

auto DemandRenderingPermission() -> void {
  std::unique_lock<std::mutex> lock(CanStartNewCommandsMutex);
  CanStartNewCommandsCV.wait(lock,
                             [&]() -> bool { return CanStartNewCommands; });
}

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
thread_local Ref<RenderThreadInfo> CurrentRenderThreadInfo;
thread_local inline std::array<std::vector<VkCommandBuffer>, FRAMES_IN_FLIGHT>
    ThreadCommandBuffers;
thread_local inline uint64_t lastThreadFrame;
thread_local inline std::vector<VkCommandBuffer> frameCommandBufferCache;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto AquireCommandBuffer(Graphics::GraphicsContext &context,
                         const AquireInfo &info)
    -> Result<Ref<RenderThreadInfo>> {

  if (!HasRenderingPermission()) {
    return Error::Unexpected(
        "Cannot aquire command buffer without rendering permission");
  }

  if (CurrentRenderThreadInfo.get() != nullptr) {
    return Error::Unexpected(
        "Current thread already has an aquired command buffer");
  }

  if (context.currentFrame != lastThreadFrame) {
    frameCommandBufferCache = ThreadCommandBuffers.at(context.frameIndex);
    lastThreadFrame = context.currentFrame;
  }

  auto threadInfo = Ref<RenderThreadInfo>::Make();
  threadInfo->currentlyRecording = true;
  threadInfo->threadData.key = std::hash<std::string>()(info.name);
  threadInfo->threadData.priority = info.priority;
#ifndef NDEBUG
  threadInfo->threadData.name = info.name;
#endif

  {
    std::lock_guard<std::mutex> lock(ResultsMutex);
    Results.emplace_back(threadInfo);
  }

  auto &tcontext = GetThreadContext();

  if (tcontext.commandPool == VK_NULL_HANDLE) {
    return Error::Unexpected(
        "Invalid command pool when aquiring command buffer");
  }

  if (context.device == VK_NULL_HANDLE) {
    return Error::Unexpected("Invalid device when aquiring command buffer");
  }

  if (frameCommandBufferCache.empty()) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = tcontext.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto allocationResult = Error::Create(vkAllocateCommandBuffers(
        context.device, &allocInfo, &threadInfo->threadData.commandBuffer));

    if (Error::IsError(allocationResult)) {
      return allocationResult.AsUnexpected();
    }
  } else {
    threadInfo->threadData.commandBuffer = frameCommandBufferCache.back();
    frameCommandBufferCache.pop_back();
  }

  // Reset old command buffer
  VkCommandBufferResetFlags resetFlags{};
  auto resetResult = Error::Create(
      vkResetCommandBuffer(threadInfo->threadData.commandBuffer, resetFlags));

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  auto beginResult = Error::Create(
      vkBeginCommandBuffer(threadInfo->threadData.commandBuffer, &beginInfo));

  if (Error::IsError(beginResult)) {
    return beginResult.AsUnexpected();
  }

  Barrier::ResetModule();

  GetThreadContext().commandBuffer = threadInfo->threadData.commandBuffer;
  CurrentRenderThreadInfo = threadInfo;

  if (GetCommandBuffer() == VK_NULL_HANDLE) {
    return Error::Unexpected("Failed to aquire command buffer.");
  }

  Graphics::SetDirtyState();

  auto result = GetGlobalUniformBuffer(context.frameIndex).NewFrame(context);

  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  return threadInfo;
}

auto SubmitCommands(Graphics::GraphicsContext &context) -> Error {
  auto validateResult = RenderTarget::FinalizeFrame(context);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }

  auto flushResult = FlushBufferUploads(context);
  if (Error::IsError(flushResult)) {
    return flushResult;
  }

  auto &threadInfo = *CurrentRenderThreadInfo;

  auto endResult =
      Error::Create(vkEndCommandBuffer(threadInfo.threadData.commandBuffer));
  if (Error::IsError(endResult)) {
    return endResult;
  }

  threadInfo.threadData.resourceSyncs = Barrier::GlobalResourceSyncTimeline;
  threadInfo.threadData.usageUpdates = Barrier::GlobalResourceStateUpdates;

  ThreadCommandBuffers.at(context.frameIndex)
      .emplace_back(threadInfo.threadData.commandBuffer);

  GetThreadContext().commandBuffer = VK_NULL_HANDLE;

  {
    std::lock_guard<std::mutex> lock(threadInfo.availabilityMutex);
    threadInfo.currentlyRecording = false;
  }
  threadInfo.availabilityCV.notify_all();
  CurrentRenderThreadInfo = {};

  return Error::Success();
}

inline auto CreateCommandPool(ThreadContext &tcontext) -> Error {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = tcontext.graphicsContext->graphicsQueueFamily;
  poolInfo.flags =
      static_cast<uint32_t>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) |
      static_cast<uint32_t>(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    Error error = Error::Create(
        vkCreateCommandPool(tcontext.graphicsContext->device, &poolInfo,
                            nullptr, &tcontext.commandPool));

    if (Error::IsError(error)) {
      return error;
    }
  }

  return Error::Success();
}

auto Initialize(Graphics::GraphicsContext &context) -> Error {
  auto &tcontext = GetThreadContext();
  tcontext.graphicsContext = &context;

  auto poolCreationResult = CreateCommandPool(tcontext);
  if (Error::IsError(poolCreationResult)) {
    return poolCreationResult;
  }

  auto error = Graphics::LoadBufferModule(context);
  if (Error::IsError(error)) {
    return error;
  }

  error = InitializeUniformBufferModule(context);
  if (Error::IsError(error)) {
    return error;
  }

  auto shaderModuleLoadResult = Graphics::Shader::LoadModule();

  if (Error::IsError(shaderModuleLoadResult)) {
    return shaderModuleLoadResult;
  }

  PrintDebug("Shader modules loaded successfully.");

  auto rendertargetLoadError = Graphics::RenderTarget::Load(context);

  if (Error::IsError(rendertargetLoadError)) {
    return rendertargetLoadError;
  }

  return Error::Success();
}

auto Deinitialize(Graphics::GraphicsContext &context) -> void {
  DeInitializeUniformBufferModule(context);
  auto err = Graphics::UnloadBufferModule(context);

  if (Error::IsError(err)) {
    PrintAlways("Error deinitializing buffer module: {}", err.message);
  }
}

} // namespace Graphics::Threading