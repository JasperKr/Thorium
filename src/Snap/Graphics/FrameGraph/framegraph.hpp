#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

struct Node {
  uint64_t depth;
  uint64_t lastEdit;
};

struct FrameGraph {
  // also defined in command.hpp
  static inline const uint64_t InvalidDepth = UINT64_MAX;

  auto Submit(const VirtualCommandBuffer &commands) -> Error;
  auto Write(VkCommandBuffer cmdBuffer) -> Error;

private:
  VirtualCommandBuffer commandBuffer;
  std::unordered_map<void *, Node> graphState;

  std::vector<std::vector<Command>> graph;

  // All resource usages in the command buffer.
  std::unordered_map<void *, std::vector<uint64_t>> resourceUsages;

  // Temporary data for Graphviz
  std::unordered_set<uint64_t> dependencies; // a | b sorted asc

  auto Compile() -> Error;
  auto BuildGraph() -> Error;
  auto Level(uint64_t idx) -> uint64_t;

  auto GetResourceStateAt(VkBuffer buffer, uint64_t time)
      -> Result<ResourceState>;
  auto GetResourceStateAt(VkImage image, uint64_t time)
      -> Result<ResourceState>;
  auto ValidateGraph() -> Error;
};
} // namespace Graphics