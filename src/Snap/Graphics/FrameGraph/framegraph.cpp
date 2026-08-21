#include "framegraph.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#if OUTPUT_DEBUG_GRAPH
#include "Modules/filesystem.hpp"
#include <format>
#endif
#include "Modules/timer.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>
#include <slang/slang.h>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto FrameGraph::Submit(const VirtualCommandBuffer &commands) -> Error {
  commandBuffer = commands;

  CHECK_ERR(Compile());

  return {};
}

auto FrameGraph::ValidateGraph() -> Error { return {}; } // NOLINT

inline auto GetReadsFromDrawState(DrawState &state) -> std::vector<void *> {
  std::vector<void *> reads;

  for (const auto &buffer : state.boundBuffers) {
    if (buffer.access != SLANG_RESOURCE_ACCESS_WRITE) {
      reads.emplace_back(buffer.buffer);
    }
  }

  for (const auto &image : state.boundImages) {
    if (image.access != SLANG_RESOURCE_ACCESS_WRITE) {
      reads.emplace_back(image.image);
    }
  }

  for (const auto &accel : state.boundAccelerationStructures) {
    reads.emplace_back(accel);
  }

  for (const auto &vertexBuffer : state.vertexBuffers) {
    if (vertexBuffer != VK_NULL_HANDLE) {
      reads.emplace_back(vertexBuffer);
    }
  }

  if (state.indexBuffer != VK_NULL_HANDLE) {
    reads.emplace_back(state.indexBuffer);
  }

  for (const auto &colorAttachment : state.colorAttachments) {
    reads.emplace_back(colorAttachment);
  }

  if (state.depthStencilAttachment != VK_NULL_HANDLE) {
    reads.emplace_back(state.depthStencilAttachment);
  }

  return reads;
}

inline auto GetReadsInternal(Command &command) -> std::vector<void *> {
  auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    return GetReadsFromDrawState(*drawState);
  }

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    return {args.srcImage};
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    return {args.texture->imageMemory->image};
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    return {args.srcBuffer};
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    return {args.srcImage};
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    return {args.srcBuffer};
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    return {args.srcImage};
  }

  return {};
}

inline auto GetWritesFromDrawState(DrawState &state) -> std::vector<void *> {
  std::vector<void *> writes;

  for (const auto &buffer : state.boundBuffers) {
    if (buffer.access == SLANG_RESOURCE_ACCESS_WRITE ||
        buffer.access == SLANG_RESOURCE_ACCESS_READ_WRITE) {
      writes.emplace_back(buffer.buffer);
    }
  }

  for (const auto &image : state.boundImages) {
    if (image.access == SLANG_RESOURCE_ACCESS_WRITE ||
        image.access == SLANG_RESOURCE_ACCESS_READ_WRITE) {
      writes.emplace_back(image.image);
    }
  }

  for (const auto &colorAttachment : state.colorAttachments) {
    writes.emplace_back(colorAttachment);
  }

  if (state.depthStencilAttachment != VK_NULL_HANDLE) {
    writes.emplace_back(state.depthStencilAttachment);
  }

  return writes;
}

inline auto GetWritesInternal(Command &command) -> std::vector<void *> {
  auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    return GetWritesFromDrawState(*drawState);
  }

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    return {args.dstImage};
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    return {args.texture->imageMemory->image};
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    return {args.dstBuffer};
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    return {args.dstImage};
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    return {args.dstImage};
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    return {args.dstBuffer};
  }

  if (std::holds_alternative<Args::VkCmdFillBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    return {args.dstBuffer};
  }

  // if (std::holds_alternative<Args::VkCmdClearAttachments>(command.data)) {
  //   const auto &args = std::get<Args::VkCmdClearAttachments>(command.data);
  // }

  return {};
}

auto FrameGraph::MapResourceUsages() -> Error {
  for (auto &command : commandBuffer.commands) {
    auto *boundState = get_if_derived<BoundResources>(command.data);

    if (boundState == nullptr) {
      continue;
    }

    {
      auto reads = std::move(GetReadsInternal(command));

      std::ranges::sort(reads);
      auto [first, last] = std::ranges::unique(reads);
      reads.erase(first, last);
      boundState->reads = std::move(reads);
    }

    {
      auto writes = std::move(GetWritesInternal(command));

      std::ranges::sort(writes);
      auto [first, last] = std::ranges::unique(writes);
      writes.erase(first, last);
      boundState->writes = std::move(writes);
    }
  }

  return {};
}

// Marks every strict ancestor of `from` with `stamp`, walking the already
// reduced parent edges. Commands older than `floor` are never expanded: any
// path between two candidate parents only passes through commands younger than
// the oldest candidate, so cutting there does not change the outcome.
auto FrameGraph::MarkAncestors(CommandID from, CommandID floor, uint32_t stamp)
    -> void {
  ancestorStack.clear();
  ancestorStack.emplace_back(from);

  while (!ancestorStack.empty()) {
    auto node = ancestorStack.back();
    ancestorStack.pop_back();

    for (const auto parent : commandParents[node]) {
      if (parent < floor || ancestorStamps[parent] == stamp) {
        continue;
      }

      ancestorStamps[parent] = stamp;
      ancestorStack.emplace_back(parent);
    }
  }
}

// Reduces `candidates` (sorted descending, no duplicates) to the maximal
// commands under reachability and stores them as the parents of `idx`.
// A candidate is dropped when another candidate already depends on it, so
//   A -> B -> C
//   D ------> C
// keeps both B -> C and D -> C, but drops A -> C.
auto FrameGraph::ReduceParents(CommandID idx,
                               const std::vector<CommandID> &candidates)
    -> void {
  auto &parents = commandParents[idx];
  parents.clear();

  if (candidates.empty()) {
    return;
  }

  // Ancestors are always older, so walking newest first guarantees that a
  // candidate reachable from another candidate is already stamped by the time
  // we get to it.
  const auto floor = candidates.back();

  for (const auto candidate : candidates) {
    if (ancestorStamps[candidate] == idx) {
      continue; // Already reachable through a candidate we kept.
    }

    parents.emplace_back(candidate);
    MarkAncestors(candidate, floor, idx);
  }
}

// NOLINTNEXTLINE
auto FrameGraph::PreCompile() -> Error {
  CommandID idx = 0;

  for (auto &command : commandBuffer.commands) {
    command.id = idx++;
    ERR_ASSERT(idx != UINT16_MAX);
  }

  const auto commandCount = commandBuffer.commands.size();

  commandParents.assign(commandCount, {});
  ancestorStamps.assign(commandCount, UINT32_MAX);
  ancestorStack.reserve(commandCount);

  // Per resource frontier: the last writer plus every read since that write.
  // Older usages are never candidates, they are reached through the frontier
  // itself (writes chain through WAW, reads before the last write are already
  // WAR parents of it).
  struct Frontier {
    CommandID lastWrite = InvalidCommandID;
    std::vector<CommandID> readsSinceWrite;
  };

  std::unordered_map<void *, Frontier> frontiers;
  std::vector<CommandID> candidates;

  for (auto &command : commandBuffer.commands) {
    const auto *bound = get_if_derived<BoundResources>(command.data);

    candidates.clear();

    if (bound != nullptr) {
      for (const auto &ptr : bound->reads) { // RAW
        auto iter = frontiers.find(ptr);

        if (iter != frontiers.end() &&
            iter->second.lastWrite != InvalidCommandID) {
          candidates.emplace_back(iter->second.lastWrite);
        }
      }

      for (const auto &ptr : bound->writes) {
        auto iter = frontiers.find(ptr);

        if (iter == frontiers.end()) {
          continue;
        }

        if (iter->second.lastWrite != InvalidCommandID) { // WAW
          candidates.emplace_back(iter->second.lastWrite);
        }

        // WAR
        candidates.append_range(iter->second.readsSinceWrite);
      }
    }

    std::ranges::sort(candidates, std::greater{});
    {
      auto [first, last] = std::ranges::unique(candidates);
      candidates.erase(first, last);
    }

    CommandLevel level = 0;
    for (const auto parent : candidates) {
      const auto parentLevel = commandBuffer.commands[parent].level;
      ERR_ASSERT(parentLevel != InvalidDepth);

      level = std::max<CommandLevel>(level, parentLevel + 1);
    }

    command.level = level;

    ReduceParents(command.id, candidates);

    if (bound != nullptr) {
      for (const auto &ptr : bound->reads) {
        frontiers[ptr].readsSinceWrite.emplace_back(command.id);
      }

      // Done after the reads so a read-write resource does not list this
      // command as its own parent on the next usage.
      for (const auto &ptr : bound->writes) {
        auto &frontier = frontiers[ptr];

        frontier.lastWrite = command.id;
        frontier.readsSinceWrite.clear();
      }
    }
  }

  return {};
}

auto FrameGraph::BuildGraph() -> Error {

  auto time = Timer::GetTime();
  CHECK_ERR(PreCompile());
  auto time2 = Timer::GetTime();

  CommandLevel maxLevel{};

  for (const auto &command : commandBuffer.commands) {
    maxLevel = std::max(maxLevel, command.level);
  }

  graph.resize(maxLevel + 1);
  for (auto i = 0; i < graph.size(); i++) {
    graph.at(i).level = i;
  }

  size_t edgeCount = 0;
  for (const auto &command : commandBuffer.commands) {
    ERR_ASSERT(command.level != InvalidDepth);
    ERR_ASSERT(command.level < maxLevel + 1);

    graph[command.level].commands.emplace_back(command.id);

#if OUTPUT_DEBUG_GRAPH
    for (const auto parent : commandParents[command.id]) {
      dependencies.emplace((uint32_t(parent) << UINT16_WIDTH) | command.id);
    }
#endif

    edgeCount += commandParents[command.id].size();
  }
  auto time3 = Timer::GetTime();

  PrintAlways("Build time: {}", time3 - time);
  PrintAlways(" - Precompile: {}", time2 - time);
  PrintAlways(" - Level: {}", time3 - time2);
  PrintAlways("Dependency edges: {}", edgeCount);

  return {};
}

#if OUTPUT_DEBUG_GRAPH
// NOLINTNEXTLINE
auto FrameGraph::DebugOutput() -> Error {
  std::stringstream stream;
  stream << "digraph FrameGraph {\n";
  stream << "rankdir=LR;\n";
  stream << R"(
    bgcolor="#1e1e1e";

    node [
        color="#888888",
        fontcolor="white",
        style="filled",
        fillcolor="#2d2d2d"
    ];

    edge [
        color="#aaaaaa",
        fontcolor="white"
    ];
  )";

  const static bool ExportRWLabels = false;

  for (const auto &level : graph) {
    stream << std::format("subgraph cluster_level_{} {{\n", level.level);
    stream << std::format("label = \"Level {}\";\n", level.level);

    for (const auto commandId : level.commands) {
      const auto &command = commandBuffer.commands.at(commandId);

      if (ExportRWLabels) {
        auto reads = GetReads(command);
        auto writes = GetWrites(command);

        std::stringstream readStr;
        std::stringstream writeStr;

        for (void *ptr : reads) {
          readStr << ptr;

          if (ptr != reads.back()) {
            readStr << "\n";
          }
        }

        for (void *ptr : writes) {
          writeStr << ptr;

          if (ptr != writes.back()) {
            writeStr << "\n";
          }
        }

        stream << std::format("{} [label=\"{}: {}\nR: {}\nW: {}\"];\n",
                              command.id, command.level,
                              CommandTypeEnumHelper.ToString(command.GetType()),
                              readStr.str(), writeStr.str());
      } else {
        stream << std::format(
            "{} [label=\"{}: {}\"];\n", command.id, command.level,
            CommandTypeEnumHelper.ToString(command.GetType()));
      }
    }

    stream << std::format("barriers_{} [\n", level.level);
    // for (const auto &barrier : level.barriers) {
    // }
    stream << std::format("shape=note,\nlabel=\"{} Barriers\"\n",
                          level.barriers.size());
    stream << "];\n}\n";
  }

  for (const auto dependency : dependencies) {
    // parentIndex << UINT16_WIDTH | idx

    uint32_t parent = (dependency >> UINT16_WIDTH) & UINT16_MAX;
    uint32_t child = dependency & UINT16_MAX;

    assert(commandBuffer.commands.at(child).level >
           commandBuffer.commands.at(parent).level);

    stream << "\"" << parent << "\" -> \"" << child << "\";\n";
  }

  stream << "}\n";

  CHECK_ERR(Filesystem::WriteFile("framegraph.dot", stream.str()));

  return {};
}
#endif

// This is probably quite broken in multiple ways
// But the idea is there

// Non inclusive start -> inclusive end
// Why? Commands are ran after barriers on a level, so if we query synced mask 5 -> 11
// Commands at 5 may cause a hazard, but 11 not if we added a barrier there beforehand.
// The barrier mask can be used with bitwise and to check if some bit is not yet synced.
// Returns the unsynced access bits as a 64x64 matrix, as access rows pipeline column
auto FrameGraph::SyncMask(CommandLevel start, CommandLevel end,
                          VkAccessFlags2 lastWriteAccess,
                          VkPipelineStageFlags2 lastWritePipeline,
                          VkAccessFlags2 dstAccess,
                          VkPipelineStageFlags2 dstPipeline)
    -> std::array<VkPipelineStageFlags2, UINT64_WIDTH> {
  start++;

  std::array<VkPipelineStageFlags2, UINT64_WIDTH> maskBits{};

  // Fast way to iterate over set bits
  for (auto bitIndex : Utils::BitIndexRange(lastWritePipeline)) {
    maskBits.at(bitIndex) = lastWriteAccess;
  }

  // mask bits are now all unsynced bits
  for (auto level = start; level <= end; level++) {
    const auto &barriers = graph.at(level).barriers;

    for (const auto &barrier : barriers) {
      for (const auto bitIndex : Utils::BitIndexRange(dstPipeline)) {
        // If this stage bit is active in this sync and we still need it
        const auto mask = 1ULL << bitIndex;

        const bool srcMatch =
            // Match the sync's source stage with our current bit
            (barrier.srcStageMask & mask) != 0U &&
            // Match the sync's source access with our resource's last usage
            (barrier.srcAccessMask & lastWriteAccess) != 0U;

        // Barrier matched destination stage
        const bool dstMatch = (barrier.dstStageMask & mask) != 0U;

        // There is no need to match dst accesses since unrelated bits do not affect the final result, say
        // we have: current: 1110, access: 0110, barrier: 1000, becomes 1110 & ~1000 -> 1110 & 0111 = 0110.
        // and since we don't care about bits not set in access, the result is unchanged.

        if (srcMatch && dstMatch) {
          // Remove the accesses that are now satisfied
          maskBits.at(bitIndex) &= ~barrier.dstAccessMask;
        }
      }
    }
  }

  return maskBits;
}

auto FrameGraph::GetRequiredBarriers(CommandID commandId, void const *resource,
                                     VkAccessFlags2 accesses,
                                     VkPipelineStageFlags2 pipelines)
    -> std::vector<VkMemoryBarrier2> {
  const auto &command = commandBuffer.commands.at(commandId);

  if (command.level == 0) {
    return {};
  }

  const auto &parents = commandParents.at(commandId);

  std::vector<VkMemoryBarrier2> memoryBarriers;

  for (const auto &parentID : parents) {
    const auto &parent = commandBuffer.commands.at(parentID);
    const auto *bound = parent.GetDrawState();

    if (bound == nullptr) {
      continue;
    }

    const auto [srcAccess, srcStage] = bound->GetStateFor(resource);

    if (srcAccess == 0U || srcStage == 0U) {
      continue;
    }

    // SyncMask returns all accesses per pipeline not yet synced
    // For the pipelines we ask about
    const auto &mask = SyncMask(parent.level, command.level, srcAccess,
                                srcStage, accesses, pipelines);

    for (const auto bit : Utils::BitIndexRange(pipelines)) {
      const auto unsyncedAccesses = mask.at(bit);

      // If for this pipeline there were still accesses unsynced for accesses we care about, we are not safe.
      if ((unsyncedAccesses & accesses) != 0U) {
        VkMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;

        // Not yet synced previously, we need to take in to account
        // all bits like write A, B, C but read only B, C, we still need to sync for A,
        // so we do not bitop away A.
        barrier.srcAccessMask = unsyncedAccesses;

        // Pipeline stage for which these accesses weren't synced
        barrier.srcStageMask = 1ULL << bit;

        // The accesses we need now
        // If we read A, B, C, but A, B is synced already
        // We can skip that sync by bitopping away A, B with the mask.
        barrier.dstAccessMask = accesses & unsyncedAccesses;
        barrier.dstStageMask = 1ULL << bit;

        memoryBarriers.emplace_back(barrier);
      }
    }
  }

  return memoryBarriers;
}

// NOLINTNEXTLINE
auto FrameGraph::ResourceAccessAt(CommandID commandId, const void *resource)
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
  const auto &command = commandBuffer.commands.at(commandId);

  const auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    return drawState->GetStateFor(resource);
  }

  const auto *boundState = get_if_derived<BoundResources>(command.data);

  if (boundState == nullptr) {
    return {0, 0};
  }

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    if (resource == args.srcImage) {
      return {VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
    if (resource == args.dstImage) {
      return {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    return {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    if (resource == args.srcBuffer) {
      return {VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
    if (resource == args.dstBuffer) {
      return {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    if (resource == args.srcImage) {
      return {VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
    if (resource == args.dstImage) {
      return {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    if (resource == args.srcBuffer) {
      return {VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
    if (resource == args.dstImage) {
      return {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    if (resource == args.srcImage) {
      return {VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
    if (resource == args.dstBuffer) {
      return {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
  }

  if (std::holds_alternative<Args::VkCmdFillBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    if (resource == args.dstBuffer) {
      return {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    }
  }

  return {0, 0};
}

inline auto MergeBarriers(std::vector<VkMemoryBarrier2> &barriers) {
  static std::vector<VkMemoryBarrier2> newBarriers;
  newBarriers.clear();

  for (const auto &barrier : barriers) {
    bool combined = false;
    for (auto &other : newBarriers) {
      if (other.srcStageMask == barrier.srcStageMask &&
          other.dstStageMask == barrier.dstStageMask) {
        other.srcAccessMask |= barrier.srcAccessMask;
        other.dstAccessMask |= barrier.dstAccessMask;

        combined = true;
        break;
      }
    }

    if (!combined) {
      newBarriers.emplace_back(barrier);
    }
  }

  barriers = newBarriers;
}

auto FrameGraph::InsertBarriers() -> Error {
  for (auto &level : graph) {
    const auto addBarriers = [&](const std::vector<void *> &resources,
                                 const CommandID commandId) -> auto {
      for (const auto *resource : resources) {
        const auto &[accesses, pipelines] =
            ResourceAccessAt(commandId, resource);

        const auto &newBarriers =
            GetRequiredBarriers(commandId, resource, accesses, pipelines);

        level.barriers.append_range(newBarriers);
      }
    };

    for (const auto commandId : level.commands) {
      const auto &command = commandBuffer.commands.at(commandId);

      const auto &reads = GetReads(command);
      const auto &writes = GetWrites(command);

      addBarriers(reads, commandId);
      addBarriers(writes, commandId);
    }

    MergeBarriers(level.barriers);
  }

  return {};
}

// NOLINTNEXTLINE
auto FrameGraph::Compile() -> Error {
  ZoneScoped;

  CHECK_ERR(ValidateGraph());
  CHECK_ERR(MapResourceUsages());
  CHECK_ERR(BuildGraph());
  CHECK_ERR(InsertBarriers());

#if OUTPUT_DEBUG_GRAPH
  CHECK_ERR(DebugOutput());
#endif

  return {};
}

auto FrameGraph::Write(VkCommandBuffer cmdBuffer) -> Error {
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmdBuffer, &beginInfo);

  for (const auto &level : graph) {
    if (!level.barriers.empty()) {
      VkDependencyInfo depInfo = {};
      depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      depInfo.memoryBarrierCount = level.barriers.size();
      depInfo.pMemoryBarriers = level.barriers.data();

      vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }
  }

  vkEndCommandBuffer(cmdBuffer);

  return {};
}

} // namespace Graphics