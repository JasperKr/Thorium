#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

using CommandLevel = uint16_t;

struct FrameGraph {
  // also defined in command.hpp
  static inline const CommandLevel InvalidDepth = UINT16_MAX;

  auto Submit(const VirtualCommandBuffer &commands) -> Error;
  auto Write(VkCommandBuffer cmdBuffer) -> Error;

private:
  VirtualCommandBuffer commandBuffer;

  std::vector<std::vector<CommandID>> graph;

  // All resource usages in the command buffer.
  std::unordered_map<void *, std::vector<CommandID>> resourceReads;
  std::unordered_map<void *, std::vector<CommandID>> resourceWrites;

  // Temporary data for Graphviz
  std::unordered_set<uint32_t> dependencies; // a | b sorted asc

  auto MapResourceUsages() -> Error;
  auto Compile() -> Error;
  auto BuildGraph() -> Error;
  auto Level(CommandID idx) -> CommandLevel;

  auto GetResourceStateAt(VkBuffer buffer, CommandID time)
      -> Result<ResourceState>;
  auto GetResourceStateAt(VkImage image, CommandID time)
      -> Result<ResourceState>;
  auto ValidateGraph() -> Error;
};
} // namespace Graphics