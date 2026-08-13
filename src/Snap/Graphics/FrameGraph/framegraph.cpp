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

auto FrameGraph::GetResourceStateAt(VkBuffer buffer, uint64_t time)
    -> Result<ResourceState> {
  auto updates = commandBuffer.bufferStateUpdates[buffer];

  auto iter = std::ranges::upper_bound(updates, time, std::less{},
                                       &decltype(updates)::value_type::second);

  return iter == updates.begin() ? ResourceState{} : std::prev(iter)->first;
}

auto FrameGraph::GetResourceStateAt(VkImage image, uint64_t time)
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

    auto reads = GetReadsInternal(command);
    boundState->reads = std::move(reads);

    auto writes = GetWritesInternal(command);
    boundState->writes = std::move(writes);
  }

  return {};
}

auto FrameGraph::Level(uint64_t idx) -> uint64_t {
  auto &command = commandBuffer.commands.at(idx);
  assert(command.id == idx);

  if (command.level != InvalidDepth) {
    return command.level;
  }

  const auto reads = GetReads(command);
  if (reads.empty()) {
    command.level = 0;
    return 0;
  }

  int64_t maxDepth = -1;

  for (const auto &ptr : reads) {
    auto &usages = resourceWrites[ptr];

    // binary search resource usage
    auto iter = std::ranges::lower_bound(usages, idx);

    if (iter == usages.end()) {
      continue;
    }

    auto index = std::distance(usages.begin(), iter); // Our usage index
    if (index == 0) {
      continue;
    }

    auto parentIndex = usages[index - 1];

    dependencies.emplace(parentIndex << UINT32_WIDTH | idx);

    auto parentDepth = static_cast<int64_t>(Level(parentIndex));

    maxDepth = std::max<int64_t>(maxDepth, parentDepth);
  }

  command.level = maxDepth + 1;

  return maxDepth + 1;
}

auto FrameGraph::BuildGraph() -> Error {
  uint64_t idx = 0;

  for (auto &command : commandBuffer.commands) {
    command.id = idx++;

    const auto &reads = GetReads(command);
    for (const auto &ptr : reads) {
      resourceReads[ptr].emplace_back(command.id);
    }

    const auto &writes = GetWrites(command);
    for (const auto &ptr : writes) {
      resourceWrites[ptr].emplace_back(command.id);
    }
  }

  uint64_t maxLevel = 0;

  for (const auto &command : commandBuffer.commands) {
    maxLevel = std::max(maxLevel, Level(command.id));
  }

  graph.resize(maxLevel + 1);

  for (const auto &command : commandBuffer.commands) {
    ERR_ASSERT(command.level != InvalidDepth);
    ERR_ASSERT(command.level < maxLevel + 1);

    graph[command.level].emplace_back(command);
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

  for (const auto &command : commandBuffer.commands) {
    stream << command.id << " [label=\""
           << CommandTypeEnumHelper.ToString(command.GetType()) << "\"];\n";
  }

  for (const auto dependency : dependencies) {
    // parentIndex << UINT32_WIDTH | idx

    uint32_t parent = (dependency >> UINT32_WIDTH) & UINT32_MAX;
    uint32_t child = dependency & UINT32_MAX;

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