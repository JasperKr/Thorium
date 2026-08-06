#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
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
  size_t currentVertexFormatHash = 0;

  // unique identifier to not the frame, but recording of command buffer
  uint64_t recordingIdentifier = 0;
  ObjectID currentMesh;

  std::vector<DescriptorPoolInfo> descriptorPools;  // Descriptor pool info
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE; // Current descriptor pool

  uint64_t timelineValue = 0;

  std::vector<std::pair<std::weak_ptr<ImageMemory>, ImageState>>
      initialImageStates;

  std::unordered_map<ObjectID, ImageState> finalImageStates;
};
} // namespace Graphics