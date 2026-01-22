#include "render.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderThread.hpp"
#include "Graphics/resource.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "buffer.hpp"
#include "dynamicRendering.hpp"
#include "graphics.hpp"
#include <cassert>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Graphics {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StitchInfo GlobalStitchInfo{};

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
  // Reset command buffers for all render threads

  // for (int i = 0; i < context.renderThreadCount; i++) {
  //   vkResetCommandBuffer(
  //       GetCommandBuffer(), // TODO: Fix for multithreading later
  //       0);
  // }

  auto &cmdBuffers = GlobalStitchInfo.commandBuffers.at(context.frameIndex);
  for (auto &cmdBuffer : cmdBuffers) {
    vkResetCommandBuffer(cmdBuffer, 0);
  }
}

static void StartRecording(Graphics::GraphicsContext &context) {
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
        PrintError("Failed to allocate stitch command buffer: {}",
                   result.ToString());
        return;
      }
    }
  }

  // Grab the first stitch command buffer for converting
  // From present to color attachment
  VkCommandBuffer cmdBuffer =
      GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  auto beginResult = Error::Create(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
  if (Error::IsError(beginResult)) {
    PrintError("Failed to begin stitch command buffer: {}",
               beginResult.ToString());
    return;
  }

  // context.commandBuffer = cmdBuffer;
  GetThreadContext().commandBuffer = cmdBuffer;
}

static auto EndRecording(Graphics::GraphicsContext &context,
                         uint32_t frameIndex) -> Error {

  // Wait for all render threads to finish recording
  for (int i = 0; i < context.renderThreadCount; i++) {

    // TODO: Wait for:
    // renderData->frameReady[frameIndex] == true;
    // for now we assume it's ready immediately since we are single threaded
  }

  // End command buffer recording
  // for (int i = 0; i < context.renderThreadCount; i++) {
  //   vkEndCommandBuffer(
  //       GetCommandBuffer()); // TODO: Fix for multithreading later
  // }

  auto currentTimelineResult =
      Graphics::GetCurrentTimelineSemaphoreValue(context);

  if (Error::IsError(currentTimelineResult)) {
    return currentTimelineResult.error();
  }

  uint64_t completedValue = currentTimelineResult.value();

  std::unordered_map<QueueID, uint64_t> completedTimelineValues = {
      {0, completedValue},
  };

  Graphics::ProcessReleasedResources(context, completedTimelineValues);

  return Error::Success();
}

void TransitionColorToPresent(VkCommandBuffer cmd, VkImage image) {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = 0,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

void TransitionPresentToColor(VkCommandBuffer cmd, VkImage image) {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

auto AquireNextSwapchainImage(Graphics::GraphicsContext &context) -> Error {
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

  if (context.imagesInFlight[context.frameIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(context.device, 1,
                    &context.imagesInFlight[context.frameIndex], VK_TRUE,
                    UINT64_MAX);
  }

  return Error::Create(vkAcquireNextImageKHR(
      context.device, context.swapchainInfo.swapchain, UINT64_MAX,
      context.swapchainImageReady[context.frameIndex], VK_NULL_HANDLE,
      &context.swapchainImageIndex));
}

auto PrepareRecording(Graphics::GraphicsContext &context) -> Error {
  ResetCommandBuffers(context);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkResetDescriptorPool(context.device,
                          context.descriptorPools.at(context.frameIndex), 0);
  }

  StartRecording(context);

  TransitionPresentToColor(
      GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(0),
      context.swapchainInfo.images[context.swapchainImageIndex]);

  return Error::Success();
}

auto WaitOnFrame(Graphics::GraphicsContext &context) -> Error {
  // Wait on swapchainImageReady semaphore
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  vkWaitForFences(context.device, 1,
                  &context.inFlightFences[context.frameIndex], VK_TRUE,
                  UINT64_MAX);
  vkResetFences(context.device, 1, &context.inFlightFences[context.frameIndex]);

  // Wait for in-flight fence for this frame
  // if (context.imagesInFlight[context.swapchainImageIndex] != VK_NULL_HANDLE) {
  //   vkWaitForFences(context.device, 1,
  //                   &context.imagesInFlight[context.swapchainImageIndex],
  //                   VK_TRUE, UINT64_MAX);
  // }

  return Error::Success();
}

auto SubmitCommandBuffers(Graphics::GraphicsContext &context,
                          const std::vector<VkCommandBuffer> &buffers,
                          size_t count) -> Error {
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  // Wait for the swapchain image ready semaphore
  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &context.swapchainImageReady[context.frameIndex];
  submitInfo.pWaitDstStageMask = &waitStage;

  submitInfo.commandBufferCount = count;
  submitInfo.pCommandBuffers = buffers.data();

  // Signal the swapchain finished semaphore
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores =
      &context.renderingFinished.at(context.swapchainImageIndex);

  // Submit
  auto err =
      Error::Create(vkQueueSubmit(context.graphicsQueue, 1, &submitInfo,
                                  context.inFlightFences[context.frameIndex]));
  if (Error::IsError(err)) {
    return err;
  }

  VkSemaphore globalTimelineSemaphore = GetGlobalTimelineSemaphore(context);
  if (globalTimelineSemaphore != VK_NULL_HANDLE) {
    uint64_t timelineValue = GetCPUTimelineSemaphoreValue(context);

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

    err = Error::Create(vkQueueSubmit(context.graphicsQueue, 1, &submitTimeline,
                                      VK_NULL_HANDLE));
    if (Error::IsError(err)) {
      return err;
    }
  }

  uint64_t &timelineValue = GetCPUTimelineSemaphoreValue(context);
  timelineValue++;

  return Error::Success();
}

auto PresentFrame(Graphics::GraphicsContext &context) -> Error {
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores =
      &context.renderingFinished.at(context.swapchainImageIndex);
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

inline auto DisallowThreadRendering() -> void {
  {
    std::lock_guard<std::mutex> lock(Threading::CanStartNewCommandsMutex);
    Threading::CanStartNewCommands = false;
  }
}

inline auto AllowThreadRendering() -> void {
  {
    std::lock_guard<std::mutex> lock(Threading::CanStartNewCommandsMutex);
    Threading::CanStartNewCommands = true;
  }
  Threading::CanStartNewCommandsCV.notify_all();
}

auto InitializeGraphics(Graphics::GraphicsContext &context) -> Error {
  auto error = AquireNextSwapchainImage(context);
  if (Error::IsError(error)) {
    return error;
  }

  StartRecording(context);

  TransitionPresentToColor(
      GetCommandBuffer(),
      context.swapchainInfo.images[context.swapchainImageIndex]);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkResetFences(context.device, 1,
                  &context.inFlightFences[context.frameIndex]);
  }

  AllowThreadRendering();

  return Error::Success();
}

inline auto
GetOrderedCommands(GraphicsContext &context,
                   std::vector<Threading::RenderThreadData> &threadRenderdatas)
    -> Error {
  std::unordered_map<uint64_t, std::vector<Threading::RenderThreadData>>
      unorderedThreadRenderdatas;

  {
    std::lock_guard<std::mutex> lock(Threading::ResultsMutex);
    for (auto &threadInfo : Threading::Results) {
      // Wait for thread to finish recording
      std::unique_lock<std::mutex> lock(threadInfo->availabilityMutex);
      while (threadInfo->currentlyRecording) {
        threadInfo->availabilityCV.wait(lock);
      }

      unorderedThreadRenderdatas[threadInfo->threadData.key].emplace_back(
          threadInfo->threadData);
    }

    Threading::Results.clear();
  }

  int idx = 0;
  for (auto key : GlobalStitchInfo.orderingKeys) {
    auto found = unorderedThreadRenderdatas.find(key);
    if (found != unorderedThreadRenderdatas.end()) {
      std::ranges::sort(found->second,
                        [](const Threading::RenderThreadData &first,
                           const Threading::RenderThreadData &second) -> bool {
                          return first.priority > second.priority;
                        });

      for (auto &data : found->second) {
        threadRenderdatas.emplace_back(data);
      }
    } else {
#ifndef NDEBUG
      PrintWarning("Missing command buffer for '{}' (key: {})",
                   GlobalStitchInfo.orderingNames.at(idx), key);
#else
      PrintWarning("Missing command buffer for key {}", key);
#endif
    }

    idx++;
  }

#ifndef NDEBUG
  GlobalStitchInfo.orderingNames.clear();

  // Check for any graphics work that was not included in the ordering keys
  for (auto &[key, datas] : unorderedThreadRenderdatas) {
    auto iter = std::ranges::find(GlobalStitchInfo.orderingKeys, key);
    if (iter == GlobalStitchInfo.orderingKeys.end()) {
      PrintWarning("Thread command buffer '{}' Is never used.", datas[0].name);
    }
  }
#endif

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
  GlobalStitchInfo.orderingNames.clear();

  return Error::Success();
}

auto Present(Graphics::GraphicsContext &context) -> Error {
  auto validateResult = RenderTarget::FinalizeFrame(context);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }

  // Do note that the user must have started all async recording before calling this
  // If the user tries to start recording after this point it will be part of the next frame
  DisallowThreadRendering();

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

  // all thread command buffers + barrier command buffers + 1 present transition
  finalCommandBuffers.resize((threadRenderdatas.size() * 2) + 1);

  size_t index = 0;

  for (size_t i = 0; i < threadRenderdatas.size(); i++) {
    assert(i < threadRenderdatas.size());
    assert(context.frameIndex < GlobalStitchInfo.commandBuffers.size());
    assert(i < GlobalStitchInfo.commandBuffers.at(context.frameIndex).size());
    assert((i * 2) + 1 < finalCommandBuffers.size());

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

  // Start present transition command buffer
  auto *presentTransitionBuffer = finalCommandBuffers.at(index);
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(presentTransitionBuffer, &beginInfo);

  auto uploadResult = FlushBufferUploads(context);
  if (Error::IsError(uploadResult)) {
    return uploadResult;
  }

  TransitionColorToPresent(
      GetCommandBuffer(),
      context.swapchainInfo.images[context.swapchainImageIndex]);

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
  context.frameIndex = context.currentFrame % FRAMES_IN_FLIGHT;
  Barrier::ResetFrameTimeline();

  auto error = AquireNextSwapchainImage(context);
  if (Error::IsError(error)) {
    return error;
  }

  // Wait on frame fences, so we can use the command buffers
  auto waitResult = WaitOnFrame(context);
  if (Error::IsError(waitResult)) {
    return waitResult;
  }

  error = PrepareRecording(context);

  if (Error::IsError(error)) {
    return error;
  }

  Graphics::SetDirtyState();
  error = RenderTarget::BeginFrame(context);
  if (Error::IsError(error)) {
    return error;
  }

  auto result = GetGlobalUniformBuffer(context.frameIndex).NewFrame(context);

  if (Error::IsError(result)) {
    return result;
  }

  AllowThreadRendering();

  return Error::Success();
}
} // namespace Graphics