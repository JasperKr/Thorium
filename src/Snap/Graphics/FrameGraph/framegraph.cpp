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
#include <sstream>
#include <unordered_map>
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

auto FrameGraph::Level(uint64_t idx) -> uint64_t {
  auto &command = commandBuffer.commands.at(idx);
  assert(command.id == idx);

  if (command.level != InvalidDepth) {
    return command.level;
  }

  const auto boundResources = GetDependencies(command);
  if (boundResources.empty()) {
    command.level = 0;

    return 0;
  }

  int64_t maxDepth = -1;

  for (const auto &ptr : boundResources) {
    auto &usages = resourceUsages[ptr];

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

    const auto &boundResources = GetDependencies(command);
    if (boundResources.empty()) {
      continue;
    }

    for (const auto &ptr : boundResources) {
      resourceUsages[ptr].emplace_back(command.id);
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
    std::visit(
        [cmdBuffer](const auto &cmdData) -> void { cmdData.Call(cmdBuffer); },
        command.data);
  }

  vkEndCommandBuffer(cmdBuffer);

  return {};
}

} // namespace Graphics