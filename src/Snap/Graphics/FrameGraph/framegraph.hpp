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

  // Direct dependencies per command, after transitive reduction.
  std::vector<std::vector<CommandID>> commandParents;

  // Scratch data for the reachability walk in ReduceParents.
  std::vector<uint32_t> ancestorStamps;
  std::vector<CommandID> ancestorStack;

  // Temporary data for Graphviz
  std::unordered_set<uint32_t> dependencies; // a | b sorted asc

  auto MapResourceUsages() -> Error;
  auto PreCompile() -> Error;
  auto Compile() -> Error;
  auto BuildGraph() -> Error;
  auto MarkAncestors(CommandID from, CommandID floor, uint32_t stamp) -> void;
  auto ReduceParents(CommandID idx, const std::vector<CommandID> &candidates)
      -> void;

  auto GetResourceStateAt(VkBuffer buffer, CommandID time)
      -> Result<ResourceState>;
  auto GetResourceStateAt(VkImage image, CommandID time)
      -> Result<ResourceState>;
  auto ValidateGraph() -> Error;
};
} // namespace Graphics