#include "framegraph.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Modules/error.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
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

auto FrameGraph::GetResourceStateAt(VkImageView image, uint64_t time)
    -> Result<ResourceState> {
  auto updates = commandBuffer.imageStateUpdates[image];

  auto iter = std::ranges::upper_bound(updates, time, std::less{},
                                       &decltype(updates)::value_type::second);

  return iter == updates.begin() ? ResourceState{} : std::prev(iter)->first;
}

auto FrameGraph::Level(uint64_t idx) -> uint64_t {
  auto &command = commandBuffer.commands.at(idx);
  assert(command.id == idx);

  std::vector<uint64_t> parents;

  const auto *boundResources = command.GetBoundResources();
  if (boundResources == nullptr) {
    return 0;
  }

  int64_t maxDepth = -1;

  for (const auto &ptr : boundResources->bound) {
    int iterIdx = 0;

    for (const auto &usage : resourceUsages[ptr]) {
      if (usage == idx) {
        break;
      }

      iterIdx++;
    }

    if (iterIdx == 0) {
      continue;
    }

    maxDepth =
        std::max<int64_t>(maxDepth, static_cast<int64_t>(Level(iterIdx - 1)));
  }

  return maxDepth + 1;
}

auto FrameGraph::BuildGraph() -> Error {
  uint64_t idx = 0;

  for (auto &command : commandBuffer.commands) {
    command.id = idx++;

    const auto *boundResources = command.GetBoundResources();
    if (boundResources == nullptr) {
      continue;
    }

    for (const auto &ptr : boundResources->bound) {
      resourceUsages[ptr].emplace_back(command.id);
    }
  }

  std::vector<uint64_t> levels(commandBuffer.commands.size(), 0);
  uint64_t maxLevel = 0;

  for (const auto &command : commandBuffer.commands) {
    auto level = Level(command.id);
    levels.emplace_back(level); // Command.id == levels.size() - 1

    maxLevel = std::max(maxLevel, level);
  }

  graph.resize(maxLevel + 1);

  for (const auto &command : commandBuffer.commands) {
    graph[levels.at(command.id)].emplace_back(command);
  }

  auto commandCount = commandBuffer.commands.size();

  return {};
}

auto FrameGraph::Compile() -> Error {
  CHECK_ERR(ValidateGraph());
  CHECK_ERR(BuildGraph());

  CHECK_ERR(BuildGraph());

  return {};
}

auto FrameGraph::Write(VkCommandBuffer cmdBuffer) -> Error {
  for (const auto &command : commandBuffer.commands) {
    std::visit(
        [cmdBuffer](const auto &cmdData) -> void { cmdData.Call(cmdBuffer); },
        command.data);
  }

  return {};
}

} // namespace Graphics