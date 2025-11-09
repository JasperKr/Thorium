#pragma once

#include "graphics.hpp"
#include "texture.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics {

struct RenderingAttachmentInfo {
  VkStructureType sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  Texture::Texture *texture{};
  VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE;
  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkClearValue clearValue{};
};

auto SetCanvas(GraphicsContext &context,
               std::vector<RenderingAttachmentInfo> colorAttachments,
               RenderingAttachmentInfo *depthAttachment) -> Error::Error;
} // namespace Graphics