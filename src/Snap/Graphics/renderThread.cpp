#include "Graphics/renderThread.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"

#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Graphics::Threading {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

thread_local Ref<RenderThreadInfo> CurrentRenderThreadInfo;

std::mutex CommandBufferCacheMutex;
std::vector<std::pair<uint64_t, VkCommandBuffer>> CommandBufferCache;

inline std::atomic<uint64_t> threadDataIDCounter = 0;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto GetCachedCommandBuffer(const GraphicsContext &context)
    -> std::optional<VkCommandBuffer> {

  std::lock_guard<std::mutex> lock(CommandBufferCacheMutex);
  for (auto it = CommandBufferCache.begin(); it != CommandBufferCache.end();
       ++it) {
    if (it->first < Graphics::semaphoreManager.GetCompletedSemaphoreValue()) {
      auto *commandBuffer = it->second;
      CommandBufferCache.erase(it);
      return commandBuffer;
    }
  }

  return std::nullopt;
}

inline auto CreateDescriptorPool(ThreadContext &tcontext)
    -> Result<VkDescriptorPool> {
  constexpr uint32_t poolSize = 512;
  constexpr uint32_t sampledPoolSize = 8192;

  static const std::vector<VkDescriptorPoolSize> poolSizes = {
      {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = sampledPoolSize},
      {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = sampledPoolSize},
      {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
       .descriptorCount = sampledPoolSize},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
       .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
       .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
       .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
       .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
       .descriptorCount = poolSize},
      {.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
       .descriptorCount = poolSize}};

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.maxSets = poolSize * static_cast<uint32_t>(poolSizes.size());
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();

  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    Error error = Error::Create(
        vkCreateDescriptorPool(tcontext.graphicsContext->device, &poolInfo,
                               GetAllocationCallbacks(), &descriptorPool));

    if (Error::IsError(error)) {
      return error;
    }
  }

  return descriptorPool;
}

inline auto GetDescriptorPool(ThreadContext &tcontext) -> Error {
  // Reset descriptor sets and other per-frame data
  auto &context = *tcontext.graphicsContext;

  VkDescriptorPool pool = VK_NULL_HANDLE;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    for (auto &descriptorPoolInfo : tcontext.descriptorPools) {
      if (!Graphics::semaphoreManager.IsInUse(
              descriptorPoolInfo.lastUsedTimestamp)) {
        pool = descriptorPoolInfo.descriptorPool;
        descriptorPoolInfo.lastUsedTimestamp =
            Graphics::SemaphoreManager::GetSemaphoreValue();
        break;
      }
    }
  }

  if (pool == VK_NULL_HANDLE) {
    pool = CHECK_RES(CreateDescriptorPool(tcontext));

    tcontext.descriptorPools.push_back(
        {pool, Graphics::SemaphoreManager::GetSemaphoreValue()});

    tcontext.descriptorPool = pool;
    DynamicRendering::DescriptorSetCache.clear();
  } else {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    tcontext.descriptorPool = pool;

    CHECK_NEW_ERR(
        vkResetDescriptorPool(context.device, tcontext.descriptorPool, 0));
    DynamicRendering::DescriptorSetCache.clear();
  }

  return Error::Success();
}

auto AcquireCommandBuffer(Graphics::GraphicsContext &context,
                          const AcquireInfo &info)
    -> Result<Ref<RenderThreadInfo>> {

  if (CurrentRenderThreadInfo.get() != nullptr) {
    return Error::Unexpected(
        "Current thread already has an acquired command buffer");
  }

  auto threadInfo = Ref<RenderThreadInfo>::Make();
  threadInfo->threadData.key = std::hash<std::string>()(info.name);
  threadInfo->threadData.priority = info.priority;
  threadInfo->threadData.name = info.name;
  threadInfo->threadData.id = threadDataIDCounter.fetch_add(1);
  threadInfo->threadData.acquiredAtFrame = context.currentFrame;

  auto &tcontext = GetThreadContext();

  if (tcontext.commandPool == VK_NULL_HANDLE) {
    return Error::Unexpected(
        "Invalid command pool when aquiring command buffer");
  }

  if (context.device == VK_NULL_HANDLE) {
    return Error::Unexpected("Invalid device when aquiring command buffer");
  }

  auto cachedCmdBuffer = GetCachedCommandBuffer(context);

  if (!cachedCmdBuffer.has_value()) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = tcontext.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkAllocateCommandBuffers(
        context.device, &allocInfo, &threadInfo->threadData.commandBuffer));
  } else {
    threadInfo->threadData.commandBuffer = cachedCmdBuffer.value();
  }

  tcontext.timelineValue = Graphics::semaphoreManager.NewSemaphoreValue();
  threadInfo->threadData.cmdBufferTimelineValue = tcontext.timelineValue;
  tcontext.initialImageStates.clear();
  tcontext.finalImageStates.clear();

  // Reset old command buffer
  VkCommandBufferResetFlags resetFlags{};
  CHECK_NEW_ERR(
      vkResetCommandBuffer(threadInfo->threadData.commandBuffer, resetFlags));

  CHECK_ERR(GetDescriptorPool(tcontext));

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CHECK_NEW_ERR(
      vkBeginCommandBuffer(threadInfo->threadData.commandBuffer, &beginInfo));

  Barrier::ResetModule();

  GetThreadContext().commandBuffer = threadInfo->threadData.commandBuffer;
  CurrentRenderThreadInfo = threadInfo;

  if (GetCommandBuffer() == VK_NULL_HANDLE) {
    return Error::Unexpected("Failed to acquire command buffer.");
  }

  Graphics::SetDirtyState();
  auto frameBeginResult = Graphics::DynamicRendering::BeginFrame(context);
  if (Error::IsError(frameBeginResult)) {
    return frameBeginResult;
  }

  GetGlobalUniformBuffer(context.frameIndex).NewFrame();

  return threadInfo;
}

auto SubmitCommands(Graphics::GraphicsContext &context)
    -> Result<Ref<RenderThreadInfo>> {
  CHECK_ERR(DynamicRendering::FinalizeFrame(context));
  CHECK_ERR(FlushBufferUploads(context));
  if (!CurrentRenderThreadInfo.isValid() ||
      CurrentRenderThreadInfo->threadData.commandBuffer == VK_NULL_HANDLE) {
    return Error::Unexpected("No command buffer to submit.");
  }
  auto &threadContext = GetThreadContext();

  CHECK_NEW_ERR(
      vkEndCommandBuffer(CurrentRenderThreadInfo->threadData.commandBuffer));

  CurrentRenderThreadInfo->threadData.resourceSyncs =
      Barrier::GlobalResourceSyncTimeline;
  CurrentRenderThreadInfo->threadData.usageUpdates =
      Barrier::GlobalResourceStateUpdates;
  CurrentRenderThreadInfo->threadData.drawsToSwapchain =
      Graphics::DynamicRendering::DrawnToSwapchain;
  CurrentRenderThreadInfo->threadData.initialImageStates =
      threadContext.initialImageStates;
  CurrentRenderThreadInfo->threadData.finalImageStates =
      threadContext.finalImageStates;

  for (auto &pool : threadContext.descriptorPools) {
    if (pool.descriptorPool == threadContext.descriptorPool) {
      pool.lastUsedTimestamp = Graphics::SemaphoreManager::GetSemaphoreValue();
      break;
    }
  }

  threadContext.commandBuffer = VK_NULL_HANDLE;
  threadContext.currentVertexFormatHash = 0;
  threadContext.currentMesh = 0;

  auto threadInfo = CurrentRenderThreadInfo;
  CurrentRenderThreadInfo.reset();

  return threadInfo;
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
                            GetAllocationCallbacks(), &tcontext.commandPool));

    if (Error::IsError(error)) {
      return error;
    }
  }

  {
    std::lock_guard<std::mutex> lock(CommandPoolsMutex);
    CommandPools.emplace_back(tcontext.commandPool);
  }

  return Error::Success();
}

auto Initialize(Graphics::GraphicsContext &context) -> Error {
  auto &tcontext = GetThreadContext();
  tcontext.graphicsContext = &context;

  GlobalAllocations.RegisterNewThreadAllocations();

  PrintDebug("Creating command pool for render thread...");

  CHECK_ERR(CreateCommandPool(tcontext));
  PrintDebug("Initializing uniform buffer module...");

  CHECK_ERR(InitializeUniformBufferModule(context));

  CHECK_ERR(Graphics::DynamicRendering::Load(context));

  return Error::Success();
}

auto Deinitialize(Graphics::GraphicsContext &context) -> Error {
  DeInitializeUniformBufferModule(context);
  Graphics::UploadBuffers.clear();

  DynamicRendering::Shutdown(context);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    // TODO: Delay thread destruction until this isn't needed anymore since this is kinda bad
    CHECK_NEW_ERR(vkDeviceWaitIdle(context.device));

    for (auto &descriptorPoolInfo : GetThreadContext().descriptorPools) {
      vkDestroyDescriptorPool(context.device, descriptorPoolInfo.descriptorPool,
                              GetAllocationCallbacks());
    }

    GetThreadContext().descriptorPools.clear();
  }

  return Error::Success();
}

auto GetGraphicsConfiguration() -> GraphicsConfiguration & {
  thread_local GraphicsConfiguration config;
  return config;
}

} // namespace Graphics::Threading