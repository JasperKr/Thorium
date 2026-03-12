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
#include "Modules/utils.hpp"
#include "Modules/window.hpp"
#include "buffer.hpp"
#include "dynamicRendering.hpp"
#include "graphics.hpp"
#include <cassert>
#include <cstddef>
#include <mutex>

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../external/tracy/public/tracy/Tracy.hpp"

namespace Graphics {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StitchInfo GlobalStitchInfo{};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SwapchainManager::SwapchainManager swapchainManager;

auto UseCommands(uint64_t key) -> void {
  GlobalStitchInfo.orderingKeys.emplace_back(key);
#ifndef NDEBUG
  GlobalStitchInfo.orderingNames.emplace_back(std::to_string(key));
#endif
}

auto UseCommands(const std::string &name) -> void {
  uint64_t hash = std::hash<std::string>{}(name);
  GlobalStitchInfo.orderingKeys.emplace_back(hash);
#ifndef NDEBUG
  GlobalStitchInfo.orderingNames.emplace_back(name);
#endif
}

static void ResetCommandBuffers(Graphics::GraphicsContext &context) {
  auto &cmdBuffers = GlobalStitchInfo.commandBuffers.at(context.frameIndex);
  for (auto &cmdBuffer : cmdBuffers) {
    vkResetCommandBuffer(cmdBuffer, 0);
  }
}

static auto StartRecording(Graphics::GraphicsContext &context) -> Error {
  // Begin command buffer, so recording can start

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
      auto result = Error::Create(vkAllocateCommandBuffers(
          context.device, &allocInfo,
          &GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(0)));
      if (Error::IsError(result)) {
        return result;
      }
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

void TransitionColorToPresent(VkCommandBuffer cmd,
                              Ref<Texture::Texture> &texture) {

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
    vkWaitForFences(context.device, 1, &context.inFlight[context.frameIndex],
                    VK_TRUE, UINT64_MAX);

    vkResetFences(context.device, 1, &context.inFlight[context.frameIndex]);
  }

  {
    ZoneScopedN("Acquire next image");
    auto result = Error::Create(vkAcquireNextImageKHR(
        context.device, context.swapchainInfo.swapchain, UINT64_MAX,
        context.imageAvailable[context.frameIndex], VK_NULL_HANDLE,
        &context.swapchainImageIndex));

    if (Error::IsError(result)) {
      return result;
    }
  }

  if (context.imageInFlight[context.swapchainImageIndex] != VK_NULL_HANDLE) {
    ZoneScopedN("Wait for image in-flight fence");
    vkWaitForFences(context.device, 1,
                    &context.imageInFlight[context.swapchainImageIndex],
                    VK_TRUE, UINT64_MAX);
  }

  return Error::Success();
}

auto PrepareRecording(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;
  ResetCommandBuffers(context);

  auto result = StartRecording(context);

  if (Error::IsError(result)) {
    return result;
  }

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
      &context.imageReady.at(context.swapchainImageIndex);

  // Submit
  auto err = Error::Create(vkQueueSubmit(context.graphicsQueue, 1, &submitInfo,
                                         context.inFlight[context.frameIndex]));
  if (Error::IsError(err)) {
    return err;
  }

  auto updateResult = UpdateSemaphoreValues(context);

  if (Error::IsError(updateResult)) {
    return updateResult.error();
  }

  auto timelineValue = updateResult.value();

  {
    VkSemaphore globalTimelineSemaphore = Graphics::globalTimelineSemaphore;
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

      err = Error::Create(vkQueueSubmit(context.graphicsQueue, 1,
                                        &submitTimeline, VK_NULL_HANDLE));
      if (Error::IsError(err)) {
        return err;
      }
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
      &context.imageReady.at(context.swapchainImageIndex);
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

  auto error = AquireNextSwapchainImage(context);
  if (Error::IsError(error)) {
    return error;
  }

  auto result = StartRecording(context);

  auto *cmdBuffer = BeginSingleTimeCommands(context);

  if (Error::IsError(result)) {
    return result;
  }

  for (VkImage image : context.swapchainInfo.images) {
    VkImageMemoryBarrier2 barrier2 = {};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier2.srcStageMask = 0;
    barrier2.srcAccessMask = 0;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier2.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

    vkCmdPipelineBarrier2(cmdBuffer, &depInfo);

    context.swapchainInfo.textures[context.swapchainImageIndex]->currentLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    context.swapchainInfo.textures[context.swapchainImageIndex]->lastUsage =
        Texture::TextureUsage::Unknown;
    context.swapchainInfo.textures[context.swapchainImageIndex]
        ->lastPipelineStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  }

  EndSingleTimeCommands(context, cmdBuffer);

  return Error::Success();
}

auto DeinitilizeRendering(GraphicsContext &context) -> void {
  swapchainManager.Deinitialize(context);
}

inline auto
GetOrderedCommands(GraphicsContext &context,
                   std::vector<Threading::RenderThreadData> &threadRenderdatas)
    -> Error {
  ZoneScoped;

  std::unordered_map<uint64_t, std::vector<Threading::RenderThreadData>>
      unorderedThreadRenderdatas;

  {
    std::lock_guard<std::mutex> lock(Threading::ResultsMutex);
    for (auto &threadInfo : Threading::Results) {
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

      unorderedThreadRenderdatas[threadInfo->threadData.key].emplace_back(
          threadInfo->threadData);
    }
  }

  auto unorderedSemaphoreValues = Graphics::GetPendingTimelineValues();
  std::vector<uint64_t> orderedSemaphoreValues = {};

  int idx = 0;
  for (auto key : GlobalStitchInfo.orderingKeys) {
    auto found = unorderedThreadRenderdatas.find(key);
    if (found != unorderedThreadRenderdatas.end()) {
      std::ranges::sort(found->second,
                        [](const Threading::RenderThreadData &first,
                           const Threading::RenderThreadData &second) -> bool {
                          if (first.priority == second.priority) {
                            return first.id < second.id;
                          }
                          return first.priority > second.priority;
                        });

      for (auto &data : found->second) {
        threadRenderdatas.emplace_back(data);

        auto semaphoreIter = unorderedSemaphoreValues.find(data.commandBuffer);
        if (semaphoreIter != unorderedSemaphoreValues.end()) {
          orderedSemaphoreValues.emplace_back(semaphoreIter->second);
          unorderedSemaphoreValues.erase(semaphoreIter);
        } else {
          PrintError("No pending timeline value found for command buffer {}",
                     (void *)data.commandBuffer);
        }

        Utils::UnorderedErase(
            Threading::Results,
            [&](const Ref<Threading::RenderThreadInfo> &info) -> bool {
              return info->threadData.id == data.id;
            });
      }
    } else {
      PrintWarning("No recorded command buffer found for key {}", key);

#ifndef NDEBUG
      return Error::Createf("Missing command buffer for '{}' (key: {})",
                            GlobalStitchInfo.orderingNames.at(idx), key);
#else
      return Error::Createf("Missing command buffer for key {}", key);
#endif
    }

    idx++;
  }

  Graphics::SetPendingTimelineValues(orderedSemaphoreValues);

  // Insert a command buffer before each recorded command buffer to handle resource barriers
  // And one at the end to transition the swapchain image to present
  size_t totalCommandBuffers = threadRenderdatas.size() + 1;
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
      auto err = Error::Create(
          vkAllocateCommandBuffers(context.device, &allocInfo, writeAddress));
      if (Error::IsError(err)) {
        return err;
      }
    }
  }

  return Error::Success();
}

inline auto SubmitBarriers(
    const GraphicsContext &context,
    const std::vector<Threading::RenderThreadData> &threadRenderdatas) {
  ZoneScoped;

  for (size_t i = 0; i < threadRenderdatas.size(); i++) {
    assert(i < threadRenderdatas.size());
    assert(context.frameIndex < GlobalStitchInfo.commandBuffers.size());
    assert(i < GlobalStitchInfo.commandBuffers.at(context.frameIndex).size());

    const auto &threadData = threadRenderdatas.at(i);
    auto *commandBuffer =
        GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(i);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

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

  GlobalStitchInfo.orderingKeys.clear();

#ifndef NDEBUG
  GlobalStitchInfo.orderingNames.clear();
#endif

  return Error::Success();
}

inline auto GetFinalCommandBuffers(
    const GraphicsContext &context,
    std::vector<VkCommandBuffer> &finalCommandBuffers,
    const std::vector<Threading::RenderThreadData> &threadRenderdatas)
    -> size_t {
  ZoneScoped;

  // all thread command buffers + barrier command buffers + 1 present transition
  finalCommandBuffers.resize((threadRenderdatas.size() * 2) + 1);

  size_t index = 0;

  for (size_t i = 0; i < threadRenderdatas.size(); i++) {
    assert(i < threadRenderdatas.size());
    assert(context.frameIndex < GlobalStitchInfo.commandBuffers.size());
    assert(i < GlobalStitchInfo.commandBuffers.at(context.frameIndex).size());
    assert(index + 1 < finalCommandBuffers.size());

    auto *stitchBuffer =
        GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(i);
    auto *threadBuffer = threadRenderdatas.at(i).commandBuffer;

    // If no barriers were needed for this thread, skip its barrier command buffer
    if (GlobalStitchInfo.usedCommandBuffers.at(i)) {
      finalCommandBuffers[index++] = stitchBuffer;
    }
    finalCommandBuffers[index++] = threadBuffer;
  }

  // Add final present transition command buffer
  assert(context.frameIndex < GlobalStitchInfo.commandBuffers.size());
  assert(threadRenderdatas.size() <
         GlobalStitchInfo.commandBuffers.at(context.frameIndex).size());
  finalCommandBuffers.at(index) =
      GlobalStitchInfo.commandBuffers.at(context.frameIndex)
          .at(threadRenderdatas.size());

  return index;
}

auto Present(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;

  auto validateResult = DynamicRendering::FinalizeFrame(context);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }

  std::vector<Threading::RenderThreadData> threadRenderdatas{};

  auto orderResult = GetOrderedCommands(context, threadRenderdatas);
  if (Error::IsError(orderResult)) {
    return orderResult;
  }

  auto barrierResult = SubmitBarriers(context, threadRenderdatas);
  if (Error::IsError(barrierResult)) {
    return barrierResult;
  }

  std::vector<VkCommandBuffer> finalCommandBuffers;

  size_t index =
      GetFinalCommandBuffers(context, finalCommandBuffers, threadRenderdatas);

  // Start present transition command buffer
  auto *presentTransitionBuffer = finalCommandBuffers.at(index);
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(presentTransitionBuffer, &beginInfo);
  GetThreadContext().commandBuffer = presentTransitionBuffer;

  auto uploadResult = FlushBufferUploads(context);
  if (Error::IsError(uploadResult)) {
    return uploadResult;
  }

  auto endFrameResult = swapchainManager.EndFrame(context);
  if (Error::IsError(endFrameResult)) {
    return endFrameResult;
  }

  GetThreadContext().commandBuffer = nullptr;
  vkEndCommandBuffer(presentTransitionBuffer);

  GlobalStitchInfo.usedCommandBuffers.clear();

  // Draw of this frame is done, end recording
  auto endResult = EndRecording(context, context.frameIndex);
  if (Error::IsError(endResult)) {
    return endResult;
  }

  // Submit command buffers
  auto submitResult =
      SubmitCommandBuffers(context, finalCommandBuffers, index + 1);
  if (Error::IsError(submitResult)) {
    return submitResult;
  }

  // Present the frame
  auto presentResult = PresentFrame(context);
  if (Error::IsError(presentResult)) {
    return presentResult;
  }

  // Prepare for next frame
  context.currentFrame++;
  context.frameIndex = (++context.frameIndex) % FRAMES_IN_FLIGHT;
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

  auto newFrameResult =
      swapchainManager.NewFrame(context, *windowContext, context.currentFrame);
  if (Error::IsError(newFrameResult)) {
    return newFrameResult;
  }

  auto error = AquireNextSwapchainImage(context);
  if (Error::IsError(error)) {
    return error;
  }

  error = PrepareRecording(context);

  if (Error::IsError(error)) {
    return error;
  }

  Graphics::SetDirtyState();
  error = DynamicRendering::BeginFrame(context);
  if (Error::IsError(error)) {
    return error;
  }

  GetGlobalUniformBuffer(context.frameIndex).NewFrame();

  return Error::Success();
}
} // namespace Graphics