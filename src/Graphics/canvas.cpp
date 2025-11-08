#include "canvas.hpp"
#include "Modules/error.hpp"
#include "vulkan/vulkan_core.h"
#include <vector>

namespace Graphics {
auto ValidateAttachments(
    std::vector<Graphics::RenderingAttachmentInfo> colorAttachments,
    Graphics::RenderingAttachmentInfo *depthAttachment) -> Error::Error {
  // All attachments must be:
  // - same width and height as the swapchain extent
  // - same sample count as the swapchain
  // - correct layouts

  if (colorAttachments.size() == 0) {
    // Rendering to swapchain, no need to validate
    return Error::Success();
  }

  Graphics::RenderingAttachmentInfo firstColorAttachment =
      colorAttachments.at(0);
  VkExtent2D expectedExtent = // VkExtent3D -> VkExtent2D
      {firstColorAttachment.texture->size.width,
       firstColorAttachment.texture->size.height};

  for (auto attachment : colorAttachments) {
    if (attachment.texture->size.width != expectedExtent.width ||
        attachment.texture->size.height != expectedExtent.height) {
      return Error::Create(
          "Color attachment size does not match expected extent.");
    }
  }

  if (depthAttachment->texture != nullptr) {
    if (depthAttachment->texture->size.width != expectedExtent.width ||
        depthAttachment->texture->size.height != expectedExtent.height) {
      return Error::Create(
          "Depth attachment size does not match expected extent.");
    }
  }

  return Error::Create("All attachments validated successfully.");
}

// colorAttachments can be NULL to indicate rendering to swapchain
// viewCount is 0 when rendering to swapchain
// depthAttachment can be NULL
auto SetCanvas(GraphicsContext &context,
               std::vector<RenderingAttachmentInfo> colorAttachments,
               RenderingAttachmentInfo *depthAttachment) -> Error::Error {
  static bool ReadyForPresent = true;
  static VkRenderingAttachmentInfo DefaultSwapchainAttachment = {};

  static std::vector<RenderingAttachmentInfo> PreviousColorAttachments = {};
  static uint32_t PreviousViewCount = 0;
  static RenderingAttachmentInfo *PreviousDepthAttachment = nullptr;
  // New frames require a new begin/end even if attachments are the same
  static uint64_t PreviousFrame = UINT32_MAX;

  VkRenderingInfo renderingInfo = {};

  if (PreviousViewCount == colorAttachments.size() &&
      PreviousDepthAttachment == depthAttachment &&
      PreviousFrame == context.currentFrame) {

    bool same = true;

    for (uint32_t i = 0; i < colorAttachments.size(); i++) {
      if (PreviousColorAttachments.at(i).texture !=
          colorAttachments.at(i).texture) {
        same = false;
        break;
      }
    }

    if (same) {
      return Error::Success();
    }
  }

  // Don't end rendering on first set canvas call for a frame
  if (PreviousFrame == context.currentFrame) {
    vkCmdEndRendering(GetCommandBuffer(context, 0));
  }

  PreviousColorAttachments = colorAttachments;
  PreviousViewCount = colorAttachments.size();
  PreviousDepthAttachment = depthAttachment;
  PreviousFrame = context.currentFrame;

  Error::Error error = ValidateAttachments(colorAttachments, depthAttachment);

  if (Error::IsError(error)) {
    return error;
  }

  std::vector<VkRenderingAttachmentInfo> attachments =
      std::vector<VkRenderingAttachmentInfo>(
          context.deviceProperties.limits.maxColorAttachments);

  for (uint32_t i = 0; i < colorAttachments.size(); i++) {
    attachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachments[i].imageView = colorAttachments[i].texture->view;
    attachments[i].imageLayout = colorAttachments[i].imageLayout;
    attachments[i].loadOp = colorAttachments[i].loadOp;
    attachments[i].storeOp = colorAttachments[i].storeOp;
    attachments[i].clearValue = colorAttachments[i].clearValue;
  }

  VkRenderingAttachmentInfo depthAttachmentInfo = {};

  if (depthAttachment != nullptr && depthAttachment->texture != nullptr) {
    depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachmentInfo.imageView = depthAttachment->texture->view;
    depthAttachmentInfo.imageLayout = depthAttachment->imageLayout;
    depthAttachmentInfo.loadOp = depthAttachment->loadOp;
    depthAttachmentInfo.storeOp = depthAttachment->storeOp;
    depthAttachmentInfo.clearValue = depthAttachment->clearValue;
  }

  if (colorAttachments.size() >
      context.deviceProperties.limits.maxColorAttachments) {
    return Error::Create("Number of color attachments exceeds device limits.");
  }

  if (colorAttachments.size() == 0) {
    // Rendering to swapchain

    DefaultSwapchainAttachment.sType =
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    DefaultSwapchainAttachment.imageView =
        context.swapchainInfo.imageViews[context.swapchainImageIndex];
    DefaultSwapchainAttachment.imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    DefaultSwapchainAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR; // Never assume CLEAR, the user might want
                                     // to preserve contents and should call
                                     // Clear manually

    // But clear to red for now to debug if we're actually clearing
    DefaultSwapchainAttachment.clearValue.color =
        VkClearColorValue{{0.0F, 0.0F, 0.0F, 1.0F}};
    DefaultSwapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &DefaultSwapchainAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.renderArea.offset.x = 0;
    renderingInfo.renderArea.offset.y = 0;
    renderingInfo.renderArea.extent = context.swapchainInfo.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;

    ReadyForPresent = true;
  } else {
    VkExtent2D expectedExtent = // VkExtent3D -> VkExtent2D
        {colorAttachments[0].texture->size.width,
         colorAttachments[0].texture->size.height};

    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = attachments.data();
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.renderArea.offset.x = 0;
    renderingInfo.renderArea.offset.y = 0;
    renderingInfo.renderArea.extent = expectedExtent;

    ReadyForPresent = false; // Expect the user to set the render target back to
                             // swapchain before presenting
  }

  vkCmdBeginRendering(GetCommandBuffer(context, 0), &renderingInfo);

  return Error::Success();
}
} // namespace Graphics