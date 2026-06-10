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
#include "Wrap/Graphics/wrap_graphics.hpp"

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

  uint64_t completedValue = UINT64_MAX;

  std::lock_guard<std::mutex> lock(CommandBufferCacheMutex);
  for (auto it = CommandBufferCache.begin(); it != CommandBufferCache.end();
       ++it) {
    if (it->first <= completedValue) {
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
  };

  thread_local VkDescriptorPoolCreateInfo poolInfo = {};
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

  for (auto &descriptorPoolInfo : tcontext.descriptorPools) {
    if (!Graphics::semaphoreManager.IsInUse(
            descriptorPoolInfo.lastUsedTimestamp)) {
      pool = descriptorPoolInfo.descriptorPool;
      descriptorPoolInfo.lastUsedTimestamp =
          Graphics::semaphoreManager.GetSemaphoreValue();
      break;
    }
  }

  if (pool == VK_NULL_HANDLE) {
    auto createResult = CreateDescriptorPool(tcontext);

    if (Error::IsError(createResult)) {
      return createResult.error();
    }

    pool = createResult.value();

    tcontext.descriptorPools.push_back(
        {pool, Graphics::semaphoreManager
                   .GetSemaphoreValue()}); // Add new pool to the list

    tcontext.descriptorPool = pool;
  } else {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    tcontext.descriptorPool = pool;
    auto resetResult = Error::Create(
        vkResetDescriptorPool(context.device, tcontext.descriptorPool, 0));
    DynamicRendering::DescriptorSetCache.clear();

    if (Error::IsError(resetResult)) {
      return resetResult;
    }
  }

  return Error::Success();
}

auto AquireCommandBuffer(Graphics::GraphicsContext &context,
                         const AquireInfo &info)
    -> Result<Ref<RenderThreadInfo>> {

  if (CurrentRenderThreadInfo.get() != nullptr) {
    return Error::Unexpected(
        "Current thread already has an aquired command buffer");
  }

  auto threadInfo = Ref<RenderThreadInfo>::Make();
  threadInfo->threadData.key = std::hash<std::string>()(info.name);
  threadInfo->threadData.priority = info.priority;
  threadInfo->threadData.name = info.name;
  threadInfo->threadData.id = threadDataIDCounter.fetch_add(1);
  threadInfo->threadData.aquiredAtFrame = context.currentFrame;

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
    CHECK_ERR(Error::Create(vkAllocateCommandBuffers(
        context.device, &allocInfo, &threadInfo->threadData.commandBuffer)));
  } else {
    threadInfo->threadData.commandBuffer = cachedCmdBuffer.value();
  }

  threadInfo->threadData.cmdBufferTimelineValue =
      Graphics::semaphoreManager.NewSemaphoreValue(
          threadInfo->threadData.commandBuffer);

  // Reset old command buffer
  VkCommandBufferResetFlags resetFlags{};
  CHECK_ERR(Error::Create(
      vkResetCommandBuffer(threadInfo->threadData.commandBuffer, resetFlags)));

  auto getDescriptorPoolResult = GetDescriptorPool(tcontext);

  if (Error::IsError(getDescriptorPoolResult)) {
    return getDescriptorPoolResult;
  }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  CHECK_ERR(Error::Create(
      vkBeginCommandBuffer(threadInfo->threadData.commandBuffer, &beginInfo)));

  Barrier::ResetModule();

  GetThreadContext().commandBuffer = threadInfo->threadData.commandBuffer;
  CurrentRenderThreadInfo = threadInfo;

  if (GetCommandBuffer() == VK_NULL_HANDLE) {
    return Error::Unexpected("Failed to aquire command buffer.");
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
  auto validateResult = DynamicRendering::FinalizeFrame(context);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }

  auto flushResult = FlushBufferUploads(context);
  if (Error::IsError(flushResult)) {
    return flushResult;
  }

  CHECK_ERR(Error::Create(
      vkEndCommandBuffer(CurrentRenderThreadInfo->threadData.commandBuffer)));

  CurrentRenderThreadInfo->threadData.resourceSyncs =
      Barrier::GlobalResourceSyncTimeline;
  CurrentRenderThreadInfo->threadData.usageUpdates =
      Barrier::GlobalResourceStateUpdates;
  CurrentRenderThreadInfo->threadData.drawsToSwapchain =
      Graphics::DynamicRendering::DrawnToSwapchain;

  auto &threadContext = GetThreadContext();

  threadContext.commandBuffer = VK_NULL_HANDLE;
  threadContext.currentVertexFormatHash = 0;
  threadContext.currentVertexBuffer = nullptr;
  threadContext.currentIndexBuffer = nullptr;

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

  auto poolCreationResult = CreateCommandPool(tcontext);
  if (Error::IsError(poolCreationResult)) {
    return poolCreationResult;
  }

  PrintDebug("Initializing uniform buffer module...");

  CHECK_ERR(InitializeUniformBufferModule(context));

  auto rendertargetLoadError = Graphics::DynamicRendering::Load(context);

  if (Error::IsError(rendertargetLoadError)) {
    return rendertargetLoadError;
  }

  return Error::Success();
}

auto Deinitialize(Graphics::GraphicsContext &context) -> Error {
  DeInitializeUniformBufferModule(context);

  CHECK_ERR(DynamicRendering::Shutdown(context));

  Wrap::Graphics::ShutdownWrapGraphics();

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    // TODO: Delay thread destruction until this isn't needed anymore since this will never fire if another thread is doing shit
    auto result = vkDeviceWaitIdle(context.device);
    if (Error::IsError(result)) {
      return Error::Create(result);
    }

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