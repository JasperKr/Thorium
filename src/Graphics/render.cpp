#include "Graphics/resource.hpp"
#include "Modules/console.hpp"
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

static void BeginFrame(Graphics::GraphicsContext &context) {
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

static auto EndFrame(Graphics::GraphicsContext &context, uint32_t frameIndex)
    -> Error::Error {

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

auto SubmitCommandBuffers(Graphics::GraphicsContext &context) -> Error::Error {
  // --- 1. Submit frame command buffers with binary semaphore ---
  VkSubmitInfo submitBinary = {};
  submitBinary.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  // Wait for the swapchain image ready semaphore
  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitBinary.waitSemaphoreCount = 1;
  submitBinary.pWaitSemaphores =
      &context.swapchainImageReady[context.frameIndex];
  submitBinary.pWaitDstStageMask = &waitStage;

  // Command buffers
  std::vector<VkCommandBuffer> commandBuffers(context.renderThreadCount);
  for (int i = 0; i < context.renderThreadCount; i++) {
    Graphics::RenderData renderData = GetRenderData(context, i);
    commandBuffers[i] = renderData.commandBuffers[context.frameIndex];
  }
  submitBinary.commandBufferCount = context.renderThreadCount;
  submitBinary.pCommandBuffers = commandBuffers.data();

  // Signal the swapchain finished semaphore
  submitBinary.signalSemaphoreCount = 1;
  submitBinary.pSignalSemaphores =
      &context.renderingFinished[context.frameIndex];

  // Submit
  Error::Error err =
      Error::Create(vkQueueSubmit(context.graphicsQueue, 1, &submitBinary,
                                  context.inFlightFences[context.frameIndex]));
  if (Error::IsError(err)) {
    return err;
  }

  // --- 2. Submit a dummy timeline semaphore signal for resource tracking ---
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

auto PresentFrame(Graphics::GraphicsContext &context) -> Error::Error {
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &context.renderingFinished[context.frameIndex];
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &context.swapchainInfo.swapchain;
  presentInfo.pImageIndices = &context.swapchainImageIndex;

  // Present the image
  Error::Error err =
      Error::Create(vkQueuePresentKHR(context.graphicsQueue, &presentInfo));

  if (Error::IsError(err)) {
    return err;
  }

  return Error::Success();
}

auto Present_PreDraw(Graphics::GraphicsContext &context) -> Error::Error {
  /*
  Acquire swapchain image → imageIndex.
  Wait for that image’s fence (imagesInFlight[imageIndex]).
  Reset frame fence (inFlightFences[frameIndex]).
  Reset and begin command buffer for frameIndex.
  Transition acquired image from PRESENT -> COLOR_ATTACHMENT_OPTIMAL.
  Begin rendering
  */

  // Wait on swapchainImageReady semaphore
  vkWaitForFences(context.device, 1,
                  &context.inFlightFences[context.frameIndex], VK_TRUE,
                  UINT64_MAX);
  vkResetFences(context.device, 1, &context.inFlightFences[context.frameIndex]);

  // Aquire next image
  Error::Error err = Error::Create(vkAcquireNextImageKHR(
      context.device, context.swapchainInfo.swapchain, UINT64_MAX,
      context.swapchainImageReady[context.frameIndex], VK_NULL_HANDLE,
      &context.swapchainImageIndex));

  if (Error::IsError(err)) {
    return err;
  }

  // Wait for in-flight fence for this frame
  if (context.imagesInFlight[context.swapchainImageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(context.device, 1,
                    &context.imagesInFlight[context.swapchainImageIndex],
                    VK_TRUE, UINT64_MAX);
  }

  ResetCommandBuffers(context);

  vkResetDescriptorPool(context.device,
                        context.descriptorPools.at(context.frameIndex), 0);

  BeginFrame(context);

  // Ready for new frame
  // Convert from PRESENT_SRC_KHR -> COLOR_ATTACHMENT_OPTIMAL
  TransitionPresentToColor(
      GetCommandBuffer(context, 0),
      context.swapchainInfo.images[context.swapchainImageIndex]);

  return Error::Success();
}

auto Present_PostDraw(Graphics::GraphicsContext &context) -> Error::Error {
  /*
  end rendering.
  Transition image from COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR.
  End command buffer.
  Submit command buffer with inFlightFences[frameIndex] and semaphores.
  Present.
  Set imagesInFlight[imageIndex] = inFlightFences[frameIndex].
  Increment frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT.
  */

  // End rendering
  // vkCmdEndRendering(GetCommandBuffer(context, 0));

  // Convert from COLOR_ATTACHMENT_OPTIMAL state to PRESENT_SRC_KHR
  TransitionColorToPresent(
      GetCommandBuffer(context, 0),
      context.swapchainInfo.images[context.swapchainImageIndex]);

  assert(context.frameIndex < FRAMES_IN_FLIGHT);

  if (context.swapchainImageIndex >= context.swapchainInfo.imageCount) {
    return Error::Create("Acquired image index is out of bounds.");
  }

  // End current frame recording
  auto endFrameResult = EndFrame(context, context.frameIndex);
  if (Error::IsError(endFrameResult)) {
    return endFrameResult;
  }

  auto error = SubmitCommandBuffers(context);

  if (Error::IsError(error)) {
    return error;
  }

  error = PresentFrame(context);

  if (Error::IsError(error)) {
    return error;
  }

  context.currentFrame = context.currentFrame + 1;
  context.frameIndex = context.currentFrame % Graphics::FRAMES_IN_FLIGHT;

  return Error::Success();
}

auto InitializeGraphics(Graphics::GraphicsContext &context) -> Error::Error {
  auto error = Present_PreDraw(context);

  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

auto Present(Graphics::GraphicsContext &context) -> Error::Error {
  auto validateResult = RenderTarget::FinalizeFrame(context);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }
  auto uploadResult = FlushBufferUploads(context);
  if (Error::IsError(uploadResult)) {
    return uploadResult;
  }

  auto error = Present_PostDraw(context);
  if (Error::IsError(error)) {
    return error;
  }

  error = Present_PreDraw(context);
  if (Error::IsError(error)) {
    return error;
  }

  RenderTarget::SetDirty();

  return Error::Success();
}
} // namespace Graphics