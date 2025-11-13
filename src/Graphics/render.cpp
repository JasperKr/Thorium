#include "Modules/error.hpp"
#include "graphics.hpp"
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
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(renderData.commandBuffers[context.frameIndex],
                         &beginInfo);
  }
}

static void EndFrame(Graphics::GraphicsContext &context, uint32_t frameIndex) {

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
}

void TransitionColorToPresent(VkCommandBuffer cmd, VkImage image) {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT, // reading for present
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
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT, // reading for present
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

  vkCmdPipelineBarrier(
      cmd,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // wait for prior operations
                                            // (present)
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
      1, &barrier);
}

auto SubmitCommandBuffers(Graphics::GraphicsContext &context) -> Error::Error {
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  std::vector<VkPipelineStageFlags> waitStages = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &context.swapchainImageReady[context.frameIndex];
  submitInfo.pWaitDstStageMask = waitStages.data();

  std::vector<VkCommandBuffer> commandBuffers =
      std::vector<VkCommandBuffer>(context.renderThreadCount);
  for (int i = 0; i < context.renderThreadCount; i++) {
    Graphics::RenderData renderData = GetRenderData(context, i);
    commandBuffers.at(i) = renderData.commandBuffers[context.frameIndex];
  }

  submitInfo.pCommandBuffers = commandBuffers.data();
  submitInfo.commandBufferCount = context.renderThreadCount;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &context.renderingFinished[context.frameIndex];

  // Submit to graphics queue
  Error::Error err = Error::FromVkResult(
      vkQueueSubmit(context.graphicsQueue, 1, &submitInfo,
                    context.inFlightFences[context.frameIndex]));

  if (Error::IsError(err)) {
    return err;
  }

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
  Error::Error err = Error::FromVkResult(
      vkQueuePresentKHR(context.graphicsQueue, &presentInfo));

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
  Transition acquired image from PRESENT → COLOR_ATTACHMENT_OPTIMAL.
  Begin rendering
  */

  // Wait on swapchainImageReady semaphore
  vkWaitForFences(context.device, 1,
                  &context.inFlightFences[context.frameIndex], VK_TRUE,
                  UINT64_MAX);
  vkResetFences(context.device, 1, &context.inFlightFences[context.frameIndex]);

  // Aquire next image
  Error::Error err = Error::FromVkResult(vkAcquireNextImageKHR(
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
  assert(context.swapchainImageIndex < context.swapchainInfo.imageCount);

  if (context.swapchainImageIndex >= context.swapchainInfo.imageCount) {
    return Error::Create("Acquired image index is out of bounds.");
  }

  // End current frame recording
  EndFrame(context, context.frameIndex);

  SubmitCommandBuffers(context);

  auto error = PresentFrame(context);

  if (Error::IsError(error)) {
    return error;
  }

  context.currentFrame = context.currentFrame + 1;
  context.frameIndex = context.currentFrame % Graphics::FRAMES_IN_FLIGHT;

  return Error::Success();
}

void InitializeGraphics(Graphics::GraphicsContext &context) {
  Present_PreDraw(context);
}

auto Present(Graphics::GraphicsContext &context) -> Error::Error {
  Error::Error error = Present_PostDraw(context);
  if (Error::IsError(error)) {
    return error;
  }

  error = Present_PreDraw(context);
  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}
} // namespace Graphics