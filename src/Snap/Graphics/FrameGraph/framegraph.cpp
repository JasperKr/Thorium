#include "framegraph.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Libraries/vma.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/timer.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <slang/slang.h>
#include <sstream>
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

auto FrameGraph::GetResourceStateAt(VkBuffer buffer, CommandID time)
    -> Result<ResourceState> {
  auto updates = commandBuffer.bufferStateUpdates[buffer];

  auto iter = std::ranges::upper_bound(updates, time, std::less{},
                                       &decltype(updates)::value_type::second);

  return iter == updates.begin() ? ResourceState{} : std::prev(iter)->first;
}

auto FrameGraph::GetResourceStateAt(VkImage image, CommandID time)
    -> Result<ResourceState> {
  auto updates = commandBuffer.imageStateUpdates[image];

  auto iter = std::ranges::upper_bound(updates, time, std::less{},
                                       &decltype(updates)::value_type::second);

  return iter == updates.begin() ? ResourceState{} : std::prev(iter)->first;
}

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

    const auto *bound = get_if_derived<BoundResources>(command.data);
    if (bound != nullptr) {
      for (const auto &ptr : bound->reads) {
        resourceReads[ptr].emplace_back(command.id);
      }

      for (const auto &ptr : bound->writes) {
        resourceWrites[ptr].emplace_back(command.id);
      }
    }
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

  size_t edgeCount = 0;
  for (const auto &command : commandBuffer.commands) {
    ERR_ASSERT(command.level != InvalidDepth);
    ERR_ASSERT(command.level < maxLevel + 1);

    graph[command.level].emplace_back(command.id);

    for (const auto parent : commandParents[command.id]) {
      dependencies.emplace((uint32_t(parent) << UINT16_WIDTH) | command.id);
      edgeCount++;
    }
  }
  auto time3 = Timer::GetTime();

  PrintAlways("Build time: {}", time3 - time);
  PrintAlways(" - Precompile: {}", time2 - time);
  PrintAlways(" - Level: {}", time3 - time2);
  PrintAlways("Dependency edges: {}", edgeCount);

  return {};
}

// NOLINTNEXTLINE
auto FrameGraph::Compile() -> Error {
  PrintAlways("Command count: {}", commandBuffer.commands.size());
  auto time = Timer::GetTime();
  CHECK_ERR(ValidateGraph());
  auto time2 = Timer::GetTime();
  CHECK_ERR(MapResourceUsages());
  auto time3 = Timer::GetTime();
  CHECK_ERR(BuildGraph());
  auto time4 = Timer::GetTime();

  PrintAlways("Graph depth: {}", graph.size());
  PrintAlways("Graph build time: {}", time4 - time);
  PrintAlways(" - Validate: {}", time2 - time);
  PrintAlways(" - Map usages: {}", time3 - time2);
  PrintAlways(" - Build: {}", time4 - time3);

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

  const static bool ExportRWLabels = true;

  for (const auto &command : commandBuffer.commands) {
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
      stream << std::format("{} [label=\"{}: {}\"];\n", command.id,
                            command.level,
                            CommandTypeEnumHelper.ToString(command.GetType()));
    }
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

auto FrameGraph::Write(VkCommandBuffer cmdBuffer) -> Error {
  CHECK_ERR(Error::Create("Stop"));

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmdBuffer, &beginInfo);

  for (const auto &command : commandBuffer.commands) {
    CHECK_ERR(std::visit([cmdBuffer](const auto &cmdData)
                             -> Error { return cmdData.Call(cmdBuffer); },
                         command.data));
  }

  vkEndCommandBuffer(cmdBuffer);

  return {};
}

} // namespace Graphics