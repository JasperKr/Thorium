#pragma once

#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics {
struct DescriptorPoolInfo {
  VkDescriptorPool descriptorPool;
  uint64_t lastUsedTimestamp;
};

// Per-thread context for graphics operations, even on the main thread
struct ThreadContext {
  struct GraphicsContext *graphicsContext = nullptr; // Global graphics context
  VkCommandPool commandPool = VK_NULL_HANDLE;        // Per-thread command pool
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;    // Current command buffer

  std::vector<DescriptorPoolInfo> descriptorPools;  // Descriptor pool info
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE; // Current descriptor pool
};
} // namespace Graphics