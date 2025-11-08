#include "rendergraph.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <queue>
#include <unordered_set>

namespace Graphics::Rendergraph {
auto AddRenderPass(RenderGraph &graph,
                   const std::vector<ResourceAccess> &resourceAccesses)
    -> void {
  RenderPass pass = {};
  pass.handle = static_cast<ResourceHandle>(graph.passes.size());

  for (const auto &access : resourceAccesses) {
    auto &resource = graph.resources[access.resource];

    if (access.accessType == AccessType::Read) {
      pass.readResources.push_back(access.resource);
    } else if (access.accessType == AccessType::Write) {
      pass.writeResources.push_back(access.resource);
    }
  }

  graph.passes.push_back(pass);
}

template <typename F>
static inline auto PreOrderTraversal(const CompiledPass &renderpass, F &&action)
    -> void {
  action(renderpass);

  for (const auto &renderpass : renderpass.children) {
    PreOrderTraversal(renderpass, std::forward<F>(action));
  }
}

template <typename F>
void TraversePassesLevelOrder(const RenderGraph &graph, F action) {
  struct QueueEntry {
    const CompiledPass *pass;
    size_t level;
  };

  std::queue<QueueEntry> queue;
  std::unordered_set<ResourceHandle> visited;

  queue.push({&graph.virtualRoot, 0});
  visited.insert(graph.virtualRoot.pass.handle);

  while (!queue.empty()) {
    auto [current, level] = queue.front();
    queue.pop();

    // Call user action with the level
    action(*current, level);

    // Enqueue children with level + 1
    for (auto childHandle : current->children) {
      if (visited.insert(childHandle).second) {
        queue.push(QueueEntry{&graph.compiledPasses[childHandle], level + 1});
      }
    }
  }
}

auto inline CalculateGraphCost(const RenderGraph &graph) -> uint32_t {
  std::vector<uint32_t> resourceCosts(graph.passes.size(), 0);

  // Traverse passes and sum resource costs per-layer
  size_t levelCount = 0;

  TraversePassesLevelOrder(
      graph, [&](const CompiledPass &source, size_t level) -> void {
        for (const auto &resHandle : source.pass.readResources) {
          resourceCosts[level] += graph.resources[resHandle].cost;
        }
        for (const auto &resHandle : source.pass.writeResources) {
          resourceCosts[level] += graph.resources[resHandle].cost;
        }

        levelCount = (std::max)(level + 1, levelCount);
      });

  // Find max cost layer
  uint32_t maxCost = 0;

  for (size_t i = 0; i < levelCount; i++) {
    maxCost = (std::max)(resourceCosts[i], maxCost);
  }

  return maxCost;
}

struct NodeCost {
  ResourceHandle handle;
  uint32_t cost;
  uint32_t childrenCount;
};

template <typename compare>
auto inline SelectNextPass(std::vector<NodeCost> &availablePasses, compare comp)
    -> ResourceHandle {
  return std::min_element(availablePasses.begin(), availablePasses.end(), comp)
      ->handle;
}

// Select next pass based on graph heuristic
auto inline SelectNextPass(RenderGraph &graph,
                           std::vector<NodeCost> &availablePasses)
    -> ResourceHandle {
  switch (graph.heuristic) {
  case RenderGraphHeuristic::SmallestResourceFirst:
    return SelectNextPass(
        availablePasses,
        [](const NodeCost &first, const NodeCost &second) -> bool {
          return first.cost < second.cost;
        });
  case RenderGraphHeuristic::LargestResourceFirst:
    return SelectNextPass(
        availablePasses,
        [](const NodeCost &first, const NodeCost &second) -> bool {
          return first.cost > second.cost;
        });
  case RenderGraphHeuristic::MostChildrenFirst:
    return SelectNextPass(
        availablePasses,
        [](const NodeCost &first, const NodeCost &second) -> bool {
          return first.childrenCount > second.childrenCount;
        });
  case RenderGraphHeuristic::LeastChildrenFirst:
    return SelectNextPass(
        availablePasses,
        [](const NodeCost &first, const NodeCost &second) -> bool {
          return first.childrenCount < second.childrenCount;
        });
  default:
    return SelectNextPass(
        availablePasses,
        [](const NodeCost &first, const NodeCost &second) -> bool {
          return first.cost < second.cost;
        });
  }
}

auto inline ReadyToBeScheduled(
    const RenderGraph &graph, const CompiledPass &pass,
    const std::unordered_set<ResourceHandle> &scheduledPasses) -> bool {
  // Loop over all passes and their write resources
  // And check if they have been scheduled already
  // If any of them have not been scheduled yet, return false
  for (const auto &resHandle : pass.pass.readResources) {
    for (const auto &otherPass : graph.compiledPasses) {
      if (otherPass.pass.handle == pass.pass.handle) {
        continue;
      }

      // If any of the other passes write to this resource and are not yet
      // scheduled, we cannot schedule this pass yet
      for (const auto &writtenResHandle : otherPass.pass.writeResources) {
        if (writtenResHandle == resHandle) {
          // This pass writes to a resource we read
          if (!scheduledPasses.contains(otherPass.pass.handle)) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

auto inline ScheduleNodes(RenderGraph &graph) -> void {
  // Simple topological sort with heuristic-based selection

  std::vector<NodeCost> availablePasses;
  std::unordered_set<ResourceHandle> scheduledPasses;

  for (const auto &pass : graph.virtualRoot.children) {
    const auto &compiledPass = graph.compiledPasses[pass];

    uint32_t cost = 0;
    for (const auto &resHandle : compiledPass.pass.readResources) {
      cost += graph.resources[resHandle].cost;
    }

    for (const auto &resHandle : compiledPass.pass.writeResources) {
      cost += graph.resources[resHandle].cost;
    }

    // Root nodes are always ready to be scheduled
    assert(ReadyToBeScheduled(graph, compiledPass, scheduledPasses));

    availablePasses.push_back(NodeCost{
        .handle = pass,
        .cost = cost,
        .childrenCount = static_cast<uint32_t>(compiledPass.children.size())});
  }

  graph.compiledPasses.clear();

  while (!availablePasses.empty()) {
    ResourceHandle nextPassHandle = SelectNextPass(graph, availablePasses);

    // Remove from available passes
    for (auto it = availablePasses.begin(); it != availablePasses.end(); ++it) {
      if (it->handle == nextPassHandle) {
        availablePasses.erase(it);
        break;
      }
    }

    // Add to compiled passes
    graph.compiledPasses.push_back(graph.compiledPasses[nextPassHandle]);
    scheduledPasses.insert(nextPassHandle);

    // Enqueue children
    const auto &nextCompiledPass = graph.compiledPasses[nextPassHandle];

    for (const auto &childHandle : nextCompiledPass.children) {
      const auto &childPass = graph.compiledPasses[childHandle];

      if (!ReadyToBeScheduled(graph, childPass, scheduledPasses)) {
        continue; // Not ready yet, other dependencies will schedule it later
      }

      assert(!scheduledPasses.contains(childHandle));

      uint32_t cost = 0;
      for (const auto &resHandle : childPass.pass.readResources) {
        cost += graph.resources[resHandle].cost;
      }

      for (const auto &resHandle : childPass.pass.writeResources) {
        cost += graph.resources[resHandle].cost;
      }

      availablePasses.push_back(NodeCost{
          .handle = childHandle,
          .cost = cost,
          .childrenCount = static_cast<uint32_t>(childPass.children.size())});
    }
  }
}

auto Compile(RenderGraph &graph) -> void {
  // For each resource, calculate cost

  for (auto &resource : graph.resources) {
    if (resource.type == Type::Texture) {
      auto &texInfo = std::get<TextureInfo>(resource.info);
      if (texInfo.imported) {
        resource.cost = texInfo.external.texture.sizeInBytes;
      } else {
        resource.cost =
            texInfo.transient.extent.width * texInfo.transient.extent.height *
            texInfo.transient.extent.depth *
            Graphics::Texture::GetFormatSize(texInfo.transient.format);

        bool isVolumeTexture = texInfo.transient.extent.depth > 1 &&
                               texInfo.transient.arrayLayers == 1;

        resource.cost = static_cast<uint32_t>(
            static_cast<float>(resource.cost) *
            Graphics::Texture::GetMipChainCostMultiplier(
                texInfo.transient.mipLevels, isVolumeTexture));

        resource.cost *= texInfo.transient.arrayLayers;
      }
    } else if (resource.type == Type::Buffer) {
      auto &bufInfo = std::get<BufferInfo>(resource.info);
      resource.cost = static_cast<uint32_t>(bufInfo.transient.size);
    }
  }

  // Schedule nodes based on heuristic
  ScheduleNodes(graph);
}

} // namespace Graphics::Rendergraph