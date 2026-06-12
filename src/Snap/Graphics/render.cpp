#include "render.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderThread.hpp"
#include "Graphics/resource.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Graphics/swapchainManager.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "buffer.hpp"
#include "dynamicRendering.hpp"
#include "graphics.hpp"
#include <cassert>
#include <cstddef>
#include <mutex>

#include "vulkan/vulkan_core.h"
#include <array>
#include <cstdint>
#include <vector>

#include "../external/tracy/public/tracy/Tracy.hpp"

namespace Graphics {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StitchInfo GlobalStitchInfo{};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SwapchainManager::SwapchainManager swapchainManager;

static void ResetCommandBuffers(Graphics::GraphicsContext &context) {
  auto &cmdBuffers = GlobalStitchInfo.commandBuffers.at(context.frameIndex);
  for (auto &cmdBuffer : cmdBuffers) {
    vkResetCommandBuffer(cmdBuffer, 0);
  }
}

static auto StartRecording(Graphics::GraphicsContext &context) -> Error {
  if (GlobalStitchInfo.commandBuffers.at(context.frameIndex).empty()) {
    PrintDebug("Allocating stitch command buffer for frame {}",
               context.frameIndex);
    // Allocate 1 command buffer if none exist yet
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = GetThreadContext().commandPool;
    allocInfo.commandBufferCount = 1;

    GlobalStitchInfo.commandBuffers.at(context.frameIndex).resize(1);
    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      CHECK_NEW_ERR(vkAllocateCommandBuffers(
          context.device, &allocInfo,
          &GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(0)));
    }
  }

  GetThreadContext().commandBuffer = nullptr;

  return Error::Success();
}

static auto EndRecording(Graphics::GraphicsContext &context,
                         uint32_t frameIndex) -> Error {
  ZoneScoped;
  Graphics::ProcessReleasedResources(context);

  return Error::Success();
}

void TransitionColorToPresent(VkCommandBuffer cmd, Ref<Texture> &texture) {

  VkImageMemoryBarrier2 barrier2 = {};
  barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier2.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  barrier2.srcAccessMask = VK_ACCESS_2_NONE;
  barrier2.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  barrier2.dstAccessMask = 0;
  barrier2.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier2.image = texture->image;
  barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier2.subresourceRange.baseMipLevel = 0;
  barrier2.subresourceRange.levelCount = 1;
  barrier2.subresourceRange.baseArrayLayer = 0;
  barrier2.subresourceRange.layerCount = 1;

  VkDependencyInfo depInfo = {};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &barrier2;

  vkCmdPipelineBarrier2(cmd, &depInfo);
}

void TransitionPresentToColor(VkCommandBuffer cmd, VkImage image) {
  VkImageMemoryBarrier2 barrier2 = {};
  barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  barrier2.srcAccessMask = 0;
  barrier2.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  barrier2.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier2.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier2.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier2.image = image;
  barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier2.subresourceRange.baseMipLevel = 0;
  barrier2.subresourceRange.levelCount = 1;
  barrier2.subresourceRange.baseArrayLayer = 0;
  barrier2.subresourceRange.layerCount = 1;

  VkDependencyInfo depInfo = {};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &barrier2;

  vkCmdPipelineBarrier2(cmd, &depInfo);
}

auto AquireNextSwapchainImage(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

  if (context.inFlight[context.frameIndex] != VK_NULL_HANDLE) {
    ZoneScopedN("Wait for in-flight fence");
    CHECK_ERR(Error::Create(vkWaitForFences(
        context.device, 1, &context.inFlight[context.frameIndex], VK_TRUE,
        UINT64_MAX)));

    CHECK_ERR(Error::Create(vkResetFences(
        context.device, 1, &context.inFlight[context.frameIndex])));
  }

  {
    ZoneScopedN("Acquire next image");
    auto res = (vkAcquireNextImageKHR(
        context.device, context.swapchainInfo.swapchain, UINT64_MAX,
        context.imageAvailable[context.frameIndex], VK_NULL_HANDLE,
        &context.swapchainImageIndex));

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
      // Swapchain is out of date, need to recreate
      swapchainManager.MakeDirty();
    }

    CHECK_NEW_ERR(res);
  }

  return Error::Success();
}

auto PrepareRecording(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;
  ResetCommandBuffers(context);

  CHECK_ERR(StartRecording(context));

  return Error::Success();
}

auto SubmitCommandBuffers(Graphics::GraphicsContext &context,
                          const std::vector<VkCommandBuffer> &buffers,
                          size_t count) -> Error {
  ZoneScoped;

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  // Wait for the swapchain image ready semaphore
  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &context.imageAvailable[context.frameIndex];
  submitInfo.pWaitDstStageMask = &waitStage;

  submitInfo.commandBufferCount = count;
  submitInfo.pCommandBuffers = buffers.data();

  // Signal the swapchain finished semaphore
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores =
      &context.renderFinished.at(context.swapchainImageIndex);

  {
    ZoneScopedN("Submit command buffer to queue");
    // Submit
    auto err =
        Error::Create(vkQueueSubmit(context.graphicsQueue, 1, &submitInfo,
                                    context.inFlight[context.frameIndex]));
    CHECK_ERR(err);

    // if (err.code == VK_ERROR_OUT_OF_DATE_KHR || err.code == VK_SUBOPTIMAL_KHR) {
    //   // Swapchain is out of date, need to recreate
    //   swapchainManager.MakeDirty();
    // }
  }

  auto timelineValue =
      CHECK_RES(Graphics::semaphoreManager.UpdateSemaphoreValues(context));

  {
    ZoneScopedN("Submit timeline semaphore signal");

    VkSemaphore globalTimelineSemaphore = Graphics::semaphoreManager.semaphore;
    if (globalTimelineSemaphore != VK_NULL_HANDLE) {
      VkTimelineSemaphoreSubmitInfo timelineInfo{};
      timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
      timelineInfo.signalSemaphoreValueCount = 1;
      timelineInfo.pSignalSemaphoreValues = &timelineValue;

      VkSubmitInfo submitTimeline{};
      submitTimeline.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitTimeline.pNext = &timelineInfo;
      submitTimeline.commandBufferCount = 0; // no commands needed
      submitTimeline.pCommandBuffers = nullptr;
      submitTimeline.signalSemaphoreCount = 1;
      submitTimeline.pSignalSemaphores = &globalTimelineSemaphore;

      CHECK_NEW_ERR(vkQueueSubmit(context.graphicsQueue, 1, &submitTimeline,
                                  VK_NULL_HANDLE));
    }
  }

  return Error::Success();
}

auto PresentFrame(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores =
      &context.renderFinished.at(context.swapchainImageIndex);
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &context.swapchainInfo.swapchain;
  presentInfo.pImageIndices = &context.swapchainImageIndex;

  // Present the image
  Error err =
      Error::Create(vkQueuePresentKHR(context.graphicsQueue, &presentInfo));

  if (Error::IsError(err)) {
    return err;
  }

  return Error::Success();
}

auto InitializeRendering(Graphics::GraphicsContext &context,
                         Window::WindowContext &windowContext) -> Error {
  ZoneScoped;

  auto swapchainManagerResult =
      SwapchainManager::SwapchainManager::Initialize(context, windowContext);
  if (Error::IsError(swapchainManagerResult)) {
    return swapchainManagerResult.error();
  }

  swapchainManager = swapchainManagerResult.value();

  CHECK_ERR(AquireNextSwapchainImage(context));

  CHECK_ERR(StartRecording(context));

  return Error::Success();
}

auto DeinitilizeRendering(GraphicsContext &context) -> void {
  swapchainManager.Deinitialize(context);
}

inline auto
PrepareCommands(GraphicsContext &context,
                const std::vector<Ref<Threading::RenderThreadInfo>> &commands)
    -> Error {
  ZoneScoped;

  {
    for (const auto &threadInfo : commands) {
      if (threadInfo->threadData.drawsToSwapchain &&
          threadInfo->threadData.aquiredAtFrame != context.currentFrame) {
        return Error::Createf(
            "Thread {} tried to submit a command buffer that was recorded "
            "in frame {}, but the current frame is {}. Command buffers "
            "that draw to the swapchain must be recorded and submitted "
            "in the same frame.",
            threadInfo->threadData.name, threadInfo->threadData.aquiredAtFrame,
            context.currentFrame);
      }
    }
  }

  std::vector<uint64_t> orderedSemaphoreValues = {};

  orderedSemaphoreValues.reserve(commands.size());
  for (const auto &command : commands) {
    orderedSemaphoreValues.emplace_back(
        command->threadData.cmdBufferTimelineValue);
  }

  Graphics::semaphoreManager.QueueTimelineValues(orderedSemaphoreValues);

  // Insert a command buffer before each recorded command buffer to handle resource barriers
  // And one at the end to transition the swapchain image to present
  size_t totalCommandBuffers = commands.size() + 1;
  auto &commandBuffers = GlobalStitchInfo.commandBuffers.at(context.frameIndex);

  if (commandBuffers.size() < totalCommandBuffers) {
    auto allocationSize = totalCommandBuffers - commandBuffers.size();
    auto previousSize = commandBuffers.size();
    commandBuffers.resize(totalCommandBuffers);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = GetThreadContext().commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(allocationSize);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto *writeAddress = commandBuffers.data() + previousSize;

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      CHECK_ERR(Error::Create(
          vkAllocateCommandBuffers(context.device, &allocInfo, writeAddress)));
    }
  }

  return Error::Success();
}

inline auto
SubmitBarriers(const GraphicsContext &context,
               const std::vector<Ref<Threading::RenderThreadInfo>> &commands) {
  ZoneScoped;

  for (size_t i = 0; i < commands.size(); i++) {
    assert(i < commands.size());
    assert(context.frameIndex < GlobalStitchInfo.commandBuffers.size());
    assert(i < GlobalStitchInfo.commandBuffers.at(context.frameIndex).size());

    const auto &threadData = commands.at(i)->threadData;
    auto *commandBuffer =
        GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(i);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    bool begunCmdBuffer = false;

    for (auto [resource, newUsage] : threadData.usageUpdates) {
      auto updateResult =
          Graphics::Barrier::UpdateUsageVirtual(resource, newUsage);

      if (!updateResult.has_value()) {
        continue;
      }

      if (!begunCmdBuffer) {
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        begunCmdBuffer = true;
      }

      auto &sync = updateResult.value();

      VkMemoryBarrier2 barrier = {};
      barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
      barrier.srcStageMask = sync.srcStages;
      barrier.dstStageMask = sync.dstStages;
      barrier.srcAccessMask = sync.srcAccess;
      barrier.dstAccessMask = sync.dstAccess;

      VkDependencyInfo depInfo = {};
      depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      depInfo.memoryBarrierCount = 1;
      depInfo.pMemoryBarriers = &barrier;

      vkCmdPipelineBarrier2(commandBuffer, &depInfo);
    }

    if (begunCmdBuffer) {
      vkEndCommandBuffer(commandBuffer);
    }

    GlobalStitchInfo.usedCommandBuffers.emplace_back(begunCmdBuffer);
  }

  return Error::Success();
}

inline auto GetFinalCommandBuffers(
    const GraphicsContext &context,
    std::vector<VkCommandBuffer> &finalCommandBuffers,
    const std::vector<Ref<Threading::RenderThreadInfo>> &commands) -> size_t {
  ZoneScoped;

  // all thread command buffers + barrier command buffers + 1 present transition
  finalCommandBuffers.resize((commands.size() * 2) + 1);

  size_t index = 0;

  for (size_t i = 0; i < commands.size(); i++) {
    assert(i < commands.size());
    assert(context.frameIndex < GlobalStitchInfo.commandBuffers.size());
    assert(i < GlobalStitchInfo.commandBuffers.at(context.frameIndex).size());
    assert(index + 1 < finalCommandBuffers.size());

    auto *stitchBuffer =
        GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(i);
    auto *threadBuffer = commands.at(i)->threadData.commandBuffer;

    // If no barriers were needed for this thread, skip its barrier command buffer
    if (GlobalStitchInfo.usedCommandBuffers.at(i)) {
      finalCommandBuffers[index++] = stitchBuffer;
    }
    finalCommandBuffers[index++] = threadBuffer;
  }

  // Add final present transition command buffer
  assert(context.frameIndex < GlobalStitchInfo.commandBuffers.size());
  assert(commands.size() <
         GlobalStitchInfo.commandBuffers.at(context.frameIndex).size());
  finalCommandBuffers.at(index) =
      GlobalStitchInfo.commandBuffers.at(context.frameIndex)
          .at(commands.size());

  return index;
}

auto Present(Graphics::GraphicsContext &context,
             const std::vector<Ref<Threading::RenderThreadInfo>> &commands)
    -> Error {
  ZoneScoped;

  CHECK_ERR(DynamicRendering::FinalizeFrame(context));
  CHECK_ERR(PrepareCommands(context, commands));
  CHECK_ERR(SubmitBarriers(context, commands));

  std::vector<VkCommandBuffer> finalCommandBuffers;

  size_t index = GetFinalCommandBuffers(context, finalCommandBuffers, commands);

  // Start present transition command buffer
  auto *presentTransitionBuffer = finalCommandBuffers.at(index);
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(presentTransitionBuffer, &beginInfo);
  GetThreadContext().commandBuffer = presentTransitionBuffer;

  CHECK_ERR(FlushBufferUploads(context));
  CHECK_ERR(swapchainManager.EndFrame(context));

  GetThreadContext().commandBuffer = nullptr;
  vkEndCommandBuffer(presentTransitionBuffer);

  GlobalStitchInfo.usedCommandBuffers.clear();

  // Draw of this frame is done, end recording
  CHECK_ERR(EndRecording(context, context.frameIndex));

  // Submit command buffers
  CHECK_ERR(SubmitCommandBuffers(context, finalCommandBuffers, index + 1));

  // Present the frame
  CHECK_ERR(PresentFrame(context));

  // Prepare for next frame
  context.currentFrame++;
  context.frameIndex = context.currentFrame % FRAMES_IN_FLIGHT;
  Barrier::ResetFrameTimeline();

  auto *windowContext = Window::GetWindowContext();
  if (windowContext == nullptr) {
    return Error::Create("No current window context found.");
  }

  // TODO: Improve this
  if (windowContext->swapchainOutOfDate) {
    windowContext->swapchainOutOfDate = false;
    swapchainManager.MakeDirty();
  }

  CHECK_ERR(
      swapchainManager.NewFrame(context, *windowContext, context.currentFrame));

  CHECK_ERR(AquireNextSwapchainImage(context));

  {
    std::lock_guard<std::mutex> lock(Threading::CommandBufferCacheMutex);
    for (const auto &command : commands) {
      if (command->threadData.commandBuffer != nullptr) {
        Threading::CommandBufferCache.emplace_back(
            Graphics::semaphoreManager.GetSemaphoreValue(),
            command->threadData.commandBuffer);
        command->threadData.commandBuffer = nullptr;
      }
    }
  }

  CHECK_ERR(PrepareRecording(context));

  Graphics::SetDirtyState();
  CHECK_ERR(DynamicRendering::BeginFrame(context));

  auto &buffer = GetGlobalUniformBuffer(context.frameIndex);
  buffer.NewFrame();

  return Error::Success();
}
} // namespace Graphics