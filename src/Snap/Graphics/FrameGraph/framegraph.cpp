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
    if (buffer.access == SLANG_RESOURCE_ACCESS_READ ||
        buffer.access == SLANG_RESOURCE_ACCESS_READ_WRITE) {
      reads.emplace_back(buffer.buffer);
    }
  }

  for (const auto &image : state.boundImages) {
    if (image.access == SLANG_RESOURCE_ACCESS_READ ||
        image.access == SLANG_RESOURCE_ACCESS_READ_WRITE) {
      reads.emplace_back(image.image);
    }
  }

  for (const auto &accel : state.boundAccelerationStructures) {
    reads.emplace_back(accel);
  }

  for (const auto &vertexBuffer : state.vertexBuffers) {
    reads.emplace_back(vertexBuffer);
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

inline auto GetParentIndex(CommandID idx, const std::vector<CommandID> &usages)
    -> CommandID {
  // binary search resource usage
  auto iter = std::ranges::lower_bound(usages, idx);

  // iter == usages.end() -> no write found
  // *iter != idx -> found write, but not by us
  if (iter == usages.end() || *iter != idx) {
    return UINT16_MAX;
  }

  auto index = std::distance(usages.begin(), iter); // Our usage index

  // if we are the first write, we do not have a parent.
  if (index == 0) {
    return UINT16_MAX;
  }

  return usages[index - 1];
}

auto FrameGraph::Level(CommandID idx) -> CommandLevel {
  auto &command = commandBuffer.commands.at(idx);
  assert(command.id == idx);

  if (command.level != InvalidDepth) {
    return command.level;
  }

  const auto &reads = GetReads(command);
  const auto &writes = GetWrites(command);

  CommandLevel maxDepth = 0;
  bool hasDependency = false;

  auto addDependencies = [&](const auto &resources, auto &usageMap) -> auto {
    for (const auto &ptr : resources) {
      auto &usages = usageMap[ptr];
      auto parent = GetParentIndex(idx, usages);

      if (parent == UINT16_MAX) {
        continue;
      }

      dependencies.emplace((uint32_t(parent) << UINT16_WIDTH) | idx);

      maxDepth = std::max(maxDepth, Level(parent));
      hasDependency = true;
    }
  };

  addDependencies(reads, resourceWrites);  // RAW
  addDependencies(writes, resourceWrites); // WAW
  addDependencies(writes, resourceReads);  // WAR

  command.level = hasDependency ? maxDepth + 1 : 0;

  return command.level;
}

auto FrameGraph::BuildGraph() -> Error {
  CommandID idx = 0;

  for (auto &command : commandBuffer.commands) {
    command.id = idx++;
    ERR_ASSERT(idx != UINT16_MAX);

    const auto &reads = GetReads(command);
    for (const auto &ptr : reads) {
      resourceReads[ptr].emplace_back(command.id);
    }

    const auto &writes = GetWrites(command);
    for (const auto &ptr : writes) {
      resourceWrites[ptr].emplace_back(command.id);
    }
  }

  CommandLevel maxLevel = 0;
  for (const auto &command : commandBuffer.commands) {
    maxLevel = std::max(maxLevel, Level(command.id));
  }

  graph.resize(maxLevel + 1);

  for (const auto &command : commandBuffer.commands) {
    ERR_ASSERT(command.level != InvalidDepth);
    ERR_ASSERT(command.level < maxLevel + 1);

    graph[command.level].emplace_back(command.id);
  }

  return {};
}

auto FrameGraph::Compile() -> Error {
  PrintAlways("Command count: {}", commandBuffer.commands.size());
  auto time = Timer::GetTime();

  CHECK_ERR(ValidateGraph());
  CHECK_ERR(MapResourceUsages());
  CHECK_ERR(BuildGraph());

  auto time2 = Timer::GetTime();
  PrintAlways("Graph depth: {}", graph.size());
  PrintAlways("Graph build time: {}", time2 - time);

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

  for (const auto &command : commandBuffer.commands) {
    if (ExportRWLabels) {
      auto reads = GetReads(command);
      auto writes = GetWrites(command);

      std::stringstream readStr;
      std::stringstream writeStr;

      for (void *ptr : reads) {
        readStr << ptr;
        readStr << " ";
      }

      for (void *ptr : writes) {
        writeStr << ptr;
        writeStr << " ";
      }

      stream << std::format("{} [label=\"{}\nR: {}\nW: {}\"];\n", command.id,
                            CommandTypeEnumHelper.ToString(command.GetType()),
                            readStr.str(), writeStr.str());
    } else {
      stream << std::format("{} [label=\"{}\"];\n", command.id,
                            CommandTypeEnumHelper.ToString(command.GetType()));
    }
  }

  for (const auto dependency : dependencies) {
    // parentIndex << UINT16_WIDTH | idx

    uint32_t parent = (dependency >> UINT16_WIDTH) & UINT16_MAX;
    uint32_t child = dependency & UINT16_MAX;

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