#pragma once

#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics {
struct DescriptorPoolInfo {
  VkDescriptorPool descriptorPool;
  uint64_t lastUsedTimestamp;
};

struct ThreadContext {
  struct GraphicsContext *graphicsContext; // Global graphics context
  VkCommandPool commandPool;               // Per-thread command pool
  VkCommandBuffer commandBuffer;           // Current command buffer

  std::vector<DescriptorPoolInfo> descriptorPools; // Descriptor pool info
  VkDescriptorPool descriptorPool;                 // Current descriptor pool
};
} // namespace Graphics