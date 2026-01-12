#pragma once

#include "Graphics/graphics.hpp"
#include "vulkan/vulkan_core.h"
#include <unordered_map>
#include <vector>
namespace Graphics::Barrier {

struct ResourceState {
  VkPipelineStageFlags2 stages;
  VkAccessFlags2 access;
};

// base class for buffers and textures
struct GraphicsResource {
  auto MarkUse(ResourceState state) -> void {
    LastUsedStates[state.stages] = state.access;
  }
  std::unordered_map<VkPipelineStageFlags2, VkAccessFlags2> LastUsedStates;
};

// NOLINTNEXTLINE global bullshit
extern std::vector<GraphicsResource> Resources;

auto InsertUsage(const ResourceState &usage) -> void;

auto FlushBarriers(GraphicsContext &context, const ResourceState &srcState,
                   const ResourceState &dstState) -> void;

} // namespace Graphics::Barrier