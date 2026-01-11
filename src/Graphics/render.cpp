#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/resource.hpp"
#include "Modules/error.hpp"
#include "buffer.hpp"
#include "graphics.hpp"
#include "rendertarget.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Graphics {

static void ResetCommandBuffers(Graphics::GraphicsContext &context) {
  // Reset command buffers for all render threads

  for (int i = 0; i < context.renderThreadCount; i++) {
    Graphics::RenderData renderData =
        GetRenderData(context, i); // Get render data for this thread

    vkResetCommandBuffer(renderData.commandBuffers[context.frameIndex], 0);
  }
}

static void StartRecording(Graphics::GraphicsContext &context) {
  // Begin command buffer, so recording can start

  for (int i = 0; i < context.renderThreadCount; i++) {
    Graphics::RenderData renderData =
        GetRenderData(context, i); // Get render data for this thread

    // TODO: Thread safety here?
    renderData.frameReady[context.frameIndex] = false;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(renderData.commandBuffers[context.frameIndex],
                         &beginInfo);
  }
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
  for (int i = 0; i < context.renderThreadCount; i++) {
    Graphics::RenderData renderData =
        GetRenderData(context, i); // Get render data for this thread

    vkEndCommandBuffer(renderData.commandBuffers[frameIndex]);
  }

  auto currentTimelineResult =
      Graphics::GetCurrentTimelineSemaphoreValue(context);

  if (Error::IsError(currentTimelineResult)) {
    return currentTimelineResult.error();
  }

  uint64_t completedValue = currentTimelineResult.value();

  std::unordered_map<QueueID, uint64_t> completedTimelineValues = {
      {GetCurrentThreadIndex(), completedValue},
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

  vkResetDescriptorPool(context.device,
                        context.descriptorPools.at(context.frameIndex), 0);

  StartRecording(context);

  TransitionPresentToColor(
      GetCommandBuffer(context, 0),
      context.swapchainInfo.images[context.swapchainImageIndex]);

  return Error::Success();
}

auto WaitOnFrame(Graphics::GraphicsContext &context) -> Error {
  // Wait on swapchainImageReady semaphore
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

auto SubmitCommandBuffers(Graphics::GraphicsContext &context) -> Error {
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  // Wait for the swapchain image ready semaphore
  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &context.swapchainImageReady[context.frameIndex];
  submitInfo.pWaitDstStageMask = &waitStage;

  // Command buffers
  std::vector<VkCommandBuffer> commandBuffers(context.renderThreadCount);
  for (int i = 0; i < context.renderThreadCount; i++) {
    Graphics::RenderData renderData = GetRenderData(context, i);
    commandBuffers[i] = renderData.commandBuffers[context.frameIndex];
  }
  submitInfo.commandBufferCount = context.renderThreadCount;
  submitInfo.pCommandBuffers = commandBuffers.data();

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

auto InitializeGraphics(Graphics::GraphicsContext &context) -> Error {
  auto error = AquireNextSwapchainImage(context);
  if (Error::IsError(error)) {
    return error;
  }

  StartRecording(context);

  TransitionPresentToColor(
      GetCommandBuffer(context, 0),
      context.swapchainInfo.images[context.swapchainImageIndex]);

  vkResetFences(context.device, 1, &context.inFlightFences[context.frameIndex]);

  return Error::Success();
}

auto Present(Graphics::GraphicsContext &context) -> Error {
  auto validateResult = RenderTarget::FinalizeFrame(context);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }
  auto uploadResult = FlushBufferUploads(context);
  if (Error::IsError(uploadResult)) {
    return uploadResult;
  }

  TransitionColorToPresent(
      GetCommandBuffer(context, 0),
      context.swapchainInfo.images[context.swapchainImageIndex]);

  // Draw of this frame is done, end recording
  auto endResult = EndRecording(context, context.frameIndex);
  if (Error::IsError(endResult)) {
    return endResult;
  }

  // Submit command buffers
  auto submitResult = SubmitCommandBuffers(context);
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

  return Error::Success();
}
} // namespace Graphics