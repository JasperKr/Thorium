#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

#define OUTPUT_DEBUG_GRAPH 1

namespace Graphics {

using CommandLevel = uint16_t;

struct Level {
  CommandLevel level;

  std::vector<CommandID> commands;
  std::vector<CommandID> userBarriers;

  // Barriers to be executed BEFORE these commands
  std::vector<VkMemoryBarrier2> barriers;
};

struct FrameGraph {
  // also defined in command.hpp
  static inline const CommandLevel InvalidDepth = UINT16_MAX;

  auto Submit(const VirtualCommandBuffer &commands) -> Error;
  auto Write(const GraphicsContext &context, VkCommandBuffer cmdBuffer)
      -> Error;

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

  auto ResourceAccessAt(CommandID commandId, const void *resource)
      -> std::pair<VkAccessFlags2, VkPipelineStageFlags2>;

  auto ValidateGraph() -> Error;
  auto InsertBarriers() -> Error;
  auto GetRequiredBarriers(CommandID commandId, void const *resource,
                           VkAccessFlags2 accesses,
                           VkPipelineStageFlags2 pipelines)
      -> std::vector<VkMemoryBarrier2>;
  auto SyncMask(CommandLevel start, CommandLevel end,
                VkAccessFlags2 lastWriteAccess,
                VkPipelineStageFlags2 lastWritePipeline,
                VkAccessFlags2 dstAccess, VkPipelineStageFlags2 dstPipeline)
      -> std::array<VkPipelineStageFlags2, UINT64_WIDTH>;
};
} // namespace Graphics