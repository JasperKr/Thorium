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

#define OUTPUT_DEBUG_GRAPH 0

namespace Graphics {

using CommandLevel = uint16_t;

struct Level {
  std::vector<CommandID> commands;

  // Barriers to be executed BEFORE these commands
  std::vector<VkMemoryBarrier2> barriers;
};

struct FrameGraph {
  // also defined in command.hpp
  static inline const CommandLevel InvalidDepth = UINT16_MAX;

  auto Submit(const VirtualCommandBuffer &commands) -> Error;
  auto Write(VkCommandBuffer cmdBuffer) -> Error;

private:
  VirtualCommandBuffer commandBuffer;

  std::vector<Level> graph;

  // Direct dependencies per command, after transitive reduction.
  std::vector<std::vector<CommandID>> commandParents;

  // Scratch data for the reachability walk in ReduceParents.
  std::vector<uint32_t> ancestorStamps;
  std::vector<CommandID> ancestorStack;

#if OUTPUT_DEBUG_GRAPH
  // Temporary data for Graphviz
  std::unordered_set<uint32_t> dependencies; // a | b sorted asc

  auto DebugOutput() -> Error;
#endif

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
  auto InsertBarriers() -> Error;
  auto IsAccessSafe(CommandID commandId, void const *resource,
                    VkAccessFlags2 access, VkPipelineStageFlags2 pipelines)
      -> bool;
  auto SyncedMask(CommandLevel start, CommandLevel end,
                  VkAccessFlags2 lastWriteAccess,
                  VkPipelineStageFlags2 lastWritePipeline,
                  VkAccessFlags2 dstAccess, VkPipelineStageFlags2 dstPipeline)
      -> std::array<VkPipelineStageFlags2, UINT64_WIDTH>;
};
} // namespace Graphics