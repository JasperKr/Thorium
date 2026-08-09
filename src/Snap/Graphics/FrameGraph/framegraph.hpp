#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

struct GraphItemState {
  uint64_t depth;
  uint64_t lastEdit;
};

struct FrameGraph {
  auto Submit(const VirtualCommandBuffer &commands) -> Error;
  auto Write(VkCommandBuffer cmdBuffer) -> Error;

private:
  VirtualCommandBuffer commandBuffer;
  std::unordered_map<void *, GraphItemState> graphState;

  std::vector<std::vector<Command>> graph;

  auto GetGraphItemState(VkBuffer buffer) -> GraphItemState {
    return graphState[(void *)buffer];
  }

  auto GetGraphItemState(VkImage image) -> GraphItemState {
    return graphState[(void *)image];
  }

  auto Compile() -> Error;
  auto BuildGraph() -> Error;

  auto GetResourceStateAt(VkBuffer buffer, uint64_t time)
      -> Result<ResourceState>;
  auto GetResourceStateAt(VkImage image, uint64_t time)
      -> Result<ResourceState>;
  auto ValidateGraph() -> Error;
};
} // namespace Graphics