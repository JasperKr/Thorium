#include "rendergraph.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/error.hpp"
#include "graphics.hpp"
#include "texture.hpp"
#include "tl/expected.hpp"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#ifdef WIN32
#include <minwindef.h>
#endif
#include <queue>
#include <unordered_set>

namespace Graphics::Rendergraph {
using std::max;
using std::sort;

auto AddRenderPass(RenderGraph &graph, const RenderPassDescriptor &descriptor)
    -> ResourceHandle {
  RenderPass pass = {};

  pass.handle = static_cast<ResourceHandle>(graph.passes.size());

  for (const auto &access : descriptor.resources) {
    auto &resource = graph.resources[access.resource];

    if (access.accessType == AccessType::Read) {
      pass.readResources.emplace_back(access.resource);
    } else if (access.accessType == AccessType::Write) {
      pass.writeResources.emplace_back(access.resource);
    } else if (access.accessType == (AccessType::Read | AccessType::Write)) {
      pass.readwriteResources.emplace_back(access.resource);
    }
  }

  pass.state.viewport = descriptor.viewport;
  pass.state.scissor = descriptor.scissor;
  pass.state.clearValues = descriptor.clearValues;
  pass.state.bindPoint = descriptor.bindPoint;
  pass.state.blendModes = descriptor.blendModes;
  pass.resourceBindings = descriptor.resourceBindings;

  if (pass.state.scissor.extent.width == 0 ||
      pass.state.scissor.extent.height == 0) {
    pass.state.scissor.extent.width =
        static_cast<uint32_t>(pass.state.viewport.width);
    pass.state.scissor.extent.height =
        static_cast<uint32_t>(pass.state.viewport.height);
  }

  pass.vertexShader = descriptor.vertexShader;
  pass.fragmentShader = descriptor.fragmentShader;
  pass.computeShader = descriptor.computeShader;
  pass.executeFunction = descriptor.executeFunction;

  assert(pass.state.viewport.width > 0.0F &&
         pass.state.viewport.height > 0.0F &&
         "Viewport width and height must be greater than 0");
  assert(pass.state.scissor.extent.width > 0 &&
         pass.state.scissor.extent.height > 0 &&
         "Scissor extent width and height must be greater than 0");
  assert(pass.state.scissor.offset.x >= 0 && pass.state.scissor.offset.y >= 0 &&
         "Scissor offset x and y must be non-negative");
  assert(pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ||
         pass.state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE);
  assert(descriptor.resourceBindings.size() == descriptor.resources.size() &&
         "Resource bindings size must match resources size");
  if (pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    assert((pass.vertexShader.module != VK_NULL_HANDLE &&
            pass.fragmentShader.module != VK_NULL_HANDLE) &&
           "Vertex and fragment shaders must be set for graphics pipeline");
  } else if (pass.state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    assert(pass.computeShader.module != VK_NULL_HANDLE &&
           "Compute shader must be set for compute pipeline");
  }

  graph.passes.emplace_back(pass);

  return pass.handle;
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
  // +1 to account for virtual root node
  std::vector<uint32_t> resourceCosts(graph.passes.size() + 1, 0);

  uint32_t maxCost = 0;

  // Use resource lifetimes to calculate cost at each point in time
  // Where each point is a pass in the compiled pass list

  for (const auto &resource : graph.resources) {
    // Do not ignore persistent resources, they do not make a difference based
    // on how the graph is built, but they do contribute to overall cost, and
    // it's nice to know
    for (size_t i = resource.usageLifetime.firstUseIndex;
         i <= resource.usageLifetime.lastUseIndex; i++) {
      resourceCosts[i] += resource.cost;
    }
  }

  for (const auto &cost : resourceCosts) {
    maxCost = (std::max)(cost, maxCost);
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
    const RenderGraph &graph, const RenderPass &pass,
    const std::unordered_set<ResourceHandle> &scheduledPasses) -> bool {
  // Loop over parents and check if all have been scheduled
  // NOLINTNEXTLINE
  for (const auto &parentHandle : pass.parents) {
    if (!scheduledPasses.contains(parentHandle)) {
      return false; // parent not yet scheduled
    }
  }

  return true;
}

auto inline StartNodeScheduling(RenderGraph &graph,
                                std::vector<NodeCost> &availablePasses)
    -> void {
  for (const auto &passHandle : graph.virtualRoot.children) {
    const auto &childRenderpass = graph.passes[passHandle];

    uint32_t cost = 0;
    for (const auto &resHandle : childRenderpass.readResources) {
      cost += graph.resources[resHandle].cost;
    }

    for (const auto &resHandle : childRenderpass.writeResources) {
      cost += graph.resources[resHandle].cost;
    }

    for (const auto &resHandle : childRenderpass.readwriteResources) {
      cost += graph.resources[resHandle].cost;
    }

    availablePasses.emplace_back(
        NodeCost{.handle = passHandle,
                 .cost = cost,
                 .childrenCount =
                     static_cast<uint32_t>(childRenderpass.children.size())});
  }
}

auto inline ScheduleNodes(RenderGraph &graph) -> void {
  // Simple topological sort with heuristic-based selection

  std::vector<NodeCost> availablePasses;
  std::unordered_set<ResourceHandle> scheduledPasses;

  StartNodeScheduling(graph, availablePasses);

  // Schedule root nodes to start off the chain

  while (!availablePasses.empty()) {
    ResourceHandle nextPassHandle = SelectNextPass(graph, availablePasses);

    // Remove from available passes
    for (auto it = availablePasses.begin(); it != availablePasses.end(); ++it) {
      if (it->handle == nextPassHandle) {
        availablePasses.erase(it);
        break;
      }
    }

    CompiledPass thisPass = {};
    thisPass.pass = graph.passes[nextPassHandle];
    thisPass.children = {};
    thisPass.barriersBefore = {};
    thisPass.barriersAfter = {};

    uint32_t thisIndex = graph.compiledPasses.size();

    // Loop over parents and add this pass as child
    // We cannot do this in the previous loop since compiledPasses is being
    // built And we do not know the indices beforehand
    for (const auto &parentHandle : thisPass.pass.parents) {
      graph.compiledPasses[parentHandle].children.emplace_back(
          static_cast<ResourceHandle>(thisIndex));
    }

    // Add to compiled passes
    graph.compiledPasses.emplace_back(thisPass);
    scheduledPasses.insert(nextPassHandle);

    // Enqueue children
    for (const auto &childHandle : thisPass.pass.children) {
      const auto &childPass = graph.passes[childHandle];

      if (!ReadyToBeScheduled(graph, childPass, scheduledPasses)) {
        continue; // Not ready yet, other dependencies will schedule it later
      }

      assert(!scheduledPasses.contains(childHandle));

      uint32_t cost = 0;
      for (const auto &resHandle :
           childPass.GetResources(static_cast<AccessType>(
               static_cast<uint32_t>(AccessType::Read) |
               static_cast<uint32_t>(AccessType::Write)))) {
        cost += graph.resources[resHandle].cost;
      }

      // Todo, improve cost calculation by considering resource lifetimes up
      // until this point, meaning, if we would free resources after this,
      // remove their cost from this pass's cost calculation, don't forget to
      // also make the cost an int32_t then to allow negative costs, if we
      // deallocate more than we allocate

      availablePasses.emplace_back(NodeCost{
          .handle = childHandle,
          .cost = cost,
          .childrenCount = static_cast<uint32_t>(childPass.children.size())});
    }
  }

  std::cout << "Scheduled " << graph.compiledPasses.size() << " passes."
            << "\n";
  std::cout << "----------------------------" << "\n";
  // Skip virtual root
  for (int i = 1; i < graph.compiledPasses.size(); i++) {
    std::cout << i << " (" << graph.compiledPasses[i].pass.handle << ")";
    if (i != graph.compiledPasses.size() - 1) {
      std::cout << " -> ";
    }
  }
  std::cout << "\n" << "----------------------------" << "\n";
}

static inline auto HasReadDependency(const RenderGraph &graph, size_t passIndex)
    -> bool {
  const auto &pass = graph.passes[passIndex];

  // For each resource read by this pass
  for (auto read : pass.GetResources(AccessType::Read)) {

    // Look for a pass that writes this resource
    for (size_t i = 0; i < graph.passes.size(); i++) {
      if (i == passIndex) {
        continue;
      }

      const auto &other = graph.passes[i];
      for (auto written : other.GetResources(AccessType::Write)) {
        if (written == read) {
          return true; // dependency found
        }
      }
    }
  }

  return false; // no dependencies on any written resources
}

auto BuildVirtualRoot(RenderGraph &graph) -> void {
  // Loop over all passes and find nodes with no dependencies (root nodes)
  // So we can attach them to the virtual root

  graph.virtualRoot = CompiledPass{};
  graph.virtualRoot.pass.handle = static_cast<ResourceHandle>(-1);
  graph.virtualRoot.children.clear();

  for (size_t i = 0; i < graph.passes.size(); i++) {
    if (!HasReadDependency(graph, i)) {
      graph.virtualRoot.children.emplace_back(static_cast<ResourceHandle>(i));
    }
  }

  graph.compiledPasses.clear();
  graph.compiledPasses.emplace_back(graph.virtualRoot);
}

auto BuildGraph(RenderGraph &graph) -> void {
  // For each pass, find dependencies and build child relationships

  for (size_t i = 0; i < graph.passes.size(); i++) {
    const auto &pass = graph.passes[i];

    // For each resource read by this pass
    for (auto read : pass.GetResources(AccessType::Read)) {

      // Look for a pass that writes this resource
      for (size_t j = 0; j < graph.passes.size(); j++) {
        if (j == i) {
          continue;
        }

        const auto &other = graph.passes[j];
        for (auto written : other.GetResources(AccessType::Write)) {
          if (written == read) {
            // Found a dependency, add as child
            graph.passes[j].children.emplace_back(
                static_cast<ResourceHandle>(i));
            graph.passes[i].parents.emplace_back(
                static_cast<ResourceHandle>(j));
          }
        }
      }
    }
  }
}

[[nodiscard]] auto inline ValidateCompiledGraph(const RenderGraph &graph)
    -> tl::expected<bool, Error::Error> {
  // For now just check if the last pass does not write any resources
  // It can write persistent resources since they live beyond the graph
  // execution (e.g. swapchain images, or other long-lived targets)

  const auto &lastPass = graph.compiledPasses.back().pass;

  for (const auto &resHandle : lastPass.GetResources(AccessType::Write)) {
    const auto &resource = graph.resources[resHandle];

    if (resource.lifetime == ResourceLifetime::Transient) {
      return tl::make_unexpected(
          Error::Create("Render graph validation failed: last pass writes "
                        "resource " +
                        std::to_string(resHandle)));
    }
  }

  return true;
}

auto inline CalculateResourceLifetimes(RenderGraph &graph) -> void {
  // For each resource, determine first and last usage
  // This is post-graph build, so we know all passes and their dependencies
  // And when resources are used, first/last use is the index in compiled passes
  // vector, which is sorted in order of execution

  for (auto &resource : graph.resources) {
    resource.usageLifetime.firstUseIndex = UINT16_MAX;
    resource.usageLifetime.lastUseIndex = 0;
  }

  for (uint16_t passIndex = 0;
       passIndex < static_cast<uint16_t>(graph.compiledPasses.size());
       passIndex++) {
    const auto &pass = graph.compiledPasses[passIndex];

    for (const auto &resHandle : pass.pass.readResources) {
      auto &resource = graph.resources[resHandle];
      if (resource.lifetime == ResourceLifetime::Persistent) {
        continue;
      }

      resource.usageLifetime.firstUseIndex =
          (std::min)(resource.usageLifetime.firstUseIndex, passIndex);

      resource.usageLifetime.lastUseIndex =
          (std::max)(resource.usageLifetime.lastUseIndex, passIndex);
    }

    for (const auto &resHandle : pass.pass.GetResources(AccessType::Write)) {
      auto &resource = graph.resources[resHandle];

      if (resource.lifetime == ResourceLifetime::Persistent) {
        continue;
      }

      resource.usageLifetime.firstUseIndex =
          (std::min)(resource.usageLifetime.firstUseIndex, passIndex);

      resource.usageLifetime.lastUseIndex =
          (std::max)(resource.usageLifetime.lastUseIndex, passIndex);
    }
  }

  // Log resource lifetimes, excluding resources that were never used
  for (const auto &resource : graph.resources) {
    if (resource.usageLifetime.firstUseIndex == UINT16_MAX &&
        resource.usageLifetime.lastUseIndex == 0) {
      continue; // never used
    }
  }
}

[[nodiscard]] auto inline ReserveBlock(GraphicsContext &context,
                                       RenderGraph &graph, uint32_t size = 0)
    -> Error::Error {
  MemoryBlock block = {};
  block.size = size == 0 ? graph.memoryBlockSize : size;
  block.offset = 0;

  VmaVirtualBlockCreateInfo blockCreateInfo = {};
  blockCreateInfo.size = block.size;

  Error::Error error = Error::FromVkResult(
      vmaCreateVirtualBlock(&blockCreateInfo, &block.virtualBlock));

  if (Error::IsError(error)) {
    return error;
  }

  graph.memoryBlocks.emplace_back(block);

  return Error::Success();
}

struct AllocationInfo {
  ResourceHandle handle{};
  VkDeviceSize size{};
  VkDeviceSize alignment{};
};

// Try to allocate resource in existing blocks, or create new block if needed
// If a resource is bigger than the default block size, a larger block will be
// created to fit it
[[nodiscard]] auto inline AllocateResourceInBlocks(GraphicsContext &context,
                                                   RenderGraph &graph,
                                                   AllocationInfo info)
    -> Error::Error {

  VmaVirtualAllocationCreateInfo allocInfo{};
  allocInfo.size = info.size;
  allocInfo.alignment = info.alignment;

  VmaVirtualAllocation alloc = nullptr;
  VkDeviceSize offset = 0;

  // Try to place in existing blocks
  for (uint32_t blockIndex = 0; blockIndex < graph.memoryBlocks.size();
       blockIndex++) {
    auto &block = graph.memoryBlocks[blockIndex];

    if (vmaVirtualAllocate(block.virtualBlock, &allocInfo, &alloc, &offset) ==
        VK_SUCCESS) {

      VirtualAllocation allocation = {.blockIndex = blockIndex,
                                      .resource = info.handle,
                                      .allocation = alloc,
                                      .offset = offset,
                                      .size = info.size};

      graph.virtualAllocations.try_emplace(info.handle, allocation);

      std::cout << "Allocated resource " << info.handle
                << " in new memory block of size " << info.size << " at "
                << offset << "\n";

      return Error::Success(); // success
    }
  }

  // Create larger block if needed
  uint32_t allocationSize = (std::max)(info.size, graph.memoryBlockSize);

  auto error = ReserveBlock(context, graph, allocationSize);
  if (Error::IsError(error)) {
    return error;
  }

  auto &block = graph.memoryBlocks.back();

  if (vmaVirtualAllocate(block.virtualBlock, &allocInfo, &alloc, &offset) ==
      VK_SUCCESS) {

    VirtualAllocation allocation = {
        .blockIndex = static_cast<uint32_t>(graph.memoryBlocks.size() - 1),
        .resource = info.handle,
        .allocation = alloc,
        .offset = offset,
        .size = info.size};

    graph.virtualAllocations.try_emplace(info.handle, allocation);
    std::cout << "Allocated resource " << info.handle
              << " in new memory block of size " << allocationSize << " at "
              << offset << "\n";

    return Error::Success(); // success
  }

  return Error::Create("Failed to allocate virtual memory"); // shits fucked
}

auto inline DeallocateResourceInBlocks(RenderGraph &graph,
                                       const ResourceHandle handle) -> void {
  auto allocationIterator = graph.virtualAllocations.find(handle);
  if (allocationIterator == graph.virtualAllocations.end()) {
    return; // not found
  }

  auto &allocation = allocationIterator->second;

  auto &block = graph.memoryBlocks[allocation.blockIndex];

  vmaVirtualFree(block.virtualBlock, allocation.allocation);
}

auto inline QueryMemoryAlignmentOfTexture(GraphicsContext &context,
                                          const Texture::Texture &texture)
    -> VkDeviceSize {
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = texture.format;
  imageInfo.extent = texture.size;
  imageInfo.mipLevels = texture.mipmapcount;
  imageInfo.arrayLayers = texture.arrayLayers;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = texture.usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImage tempImage = nullptr;
  VkResult result =
      vkCreateImage(context.device, &imageInfo, nullptr, &tempImage);
  if (result != VK_SUCCESS) {
    return 0; // failed to create image
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context.device, tempImage, &memRequirements);

  vkDestroyImage(context.device, tempImage, nullptr);

  return memRequirements.alignment;
}

auto inline QueryMemoryAlignmentOfBuffer(GraphicsContext &context,
                                         const Buffer &buffer) -> VkDeviceSize {
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = buffer.size;
  bufferInfo.usage = buffer.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkBuffer tempBuffer = nullptr;
  VkResult result =
      vkCreateBuffer(context.device, &bufferInfo, nullptr, &tempBuffer);
  if (result != VK_SUCCESS) {
    return 0; // failed to create buffer
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(context.device, tempBuffer, &memRequirements);

  vkDestroyBuffer(context.device, tempBuffer, nullptr);

  return memRequirements.alignment;
}

auto inline ResourceIsValid(const RenderGraph &graph,
                            const ResourceHandle handle) -> bool {
  bool handleInRange =
      handle < static_cast<ResourceHandle>(graph.resources.size());
  if (!handleInRange) {
    return false;
  }

  auto resource = graph.resources[handle];

  if (resource.usageLifetime.firstUseIndex == 0 ||
      resource.usageLifetime.firstUseIndex == UINT16_MAX) {
    return false; // Invalid first use
  }

  if (resource.usageLifetime.lastUseIndex == 0 ||
      resource.usageLifetime.lastUseIndex == UINT16_MAX) {
    return false; // Invalid last use
  }

  return true;
}

auto inline CompileResourceTimeline(RenderGraph &graph) -> void {
  // Fill in the resource timeline entries, each storing allocation/deallocation
  // events, in order of execution
  graph.compiledResources.clear();

  std::vector<std::vector<ResourceTimelineEntry>> eventsPerPass(
      graph.compiledPasses.size());

  // First, gather allocation events and sort by size descending
  // So that per-pass allocations are ordered by size

  for (const auto &resource : graph.resources) {
    if (resource.lifetime == ResourceLifetime::Persistent) {
      continue; // skip persistent resources
    }

    if (!ResourceIsValid(graph, resource.handle)) {
      continue; // skip invalid resources, e.g. never used
    }

    eventsPerPass[resource.usageLifetime.firstUseIndex].emplace_back(
        ResourceTimelineEntry{
            .resourceHandle = resource.handle,
            .passHandle = resource.usageLifetime.firstUseIndex,
            .type = ResourceTimelineEntryType::Allocate,
        });
    eventsPerPass[resource.usageLifetime.lastUseIndex].emplace_back(
        ResourceTimelineEntry{
            .resourceHandle = resource.handle,
            .passHandle = resource.usageLifetime.lastUseIndex,
            .type = ResourceTimelineEntryType::Deallocate,
        });
  }

  for (size_t passIndex = 0;
       passIndex < static_cast<size_t>(graph.compiledPasses.size());
       passIndex++) {
    auto &resources = eventsPerPass[passIndex];

    // Sort by size descending
    std::ranges::sort(
        resources,
        [&graph](const ResourceTimelineEntry &first,
                 const ResourceTimelineEntry &second) -> bool {
          // Always allocate before deallocate,
          // Since we need a new resource for a pass output
          // And deallocation happens right after the pass, since
          // it's needed during the pass, we must allocate first
          if (first.type != second.type) {
            return first.type == ResourceTimelineEntryType::Allocate;
          }

          const auto &firstRes = graph.resources[first.resourceHandle];
          const auto &secondRes = graph.resources[second.resourceHandle];

          return firstRes.cost > secondRes.cost;
        });

    // Add allocation entries
    for (const auto &resource : resources) {
      graph.compiledResources.emplace_back(resource);
      if (resource.type == ResourceTimelineEntryType::Allocate) {
        graph.compiledPasses[passIndex].allocations.emplace_back(resource);
      } else {
        graph.compiledPasses[passIndex].deallocations.emplace_back(resource);
      }
    }
  }

  std::cout << "Compiled resource timeline with "
            << graph.compiledResources.size() << " entries." << "\n";
  std::cout << "----------------------------" << "\n";

  auto lastPassHandle = static_cast<ResourceHandle>(-2);

  for (const auto &entry : graph.compiledResources) {
    if (entry.passHandle != lastPassHandle) {
      std::cout << "Pass " << entry.passHandle << ":\n";
      lastPassHandle = entry.passHandle;
    }
    std::cout << (entry.type == ResourceTimelineEntryType::Allocate
                      ? "  Allocate: "
                      : "  Deallocate: ")
              << entry.resourceHandle << "\n";
  }
  std::cout << "----------------------------" << "\n";
}

struct AttachmentInfo {
  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkClearValue clearValue{};
  BlendMode blendMode;
};

auto inline GetPassAttachmentInfo(const RenderPass &pass,
                                  const ResourceBinding &binding)
    -> AttachmentInfo {
  size_t blendmodeCount = pass.state.blendModes.size();
  size_t clearColorCount = pass.state.clearValues.size();

  BlendMode blendMode = {}; // Default blend mode: No blending, disabled.

  if (binding.location < blendmodeCount) {
    blendMode = pass.state.blendModes[binding.location];
  } else if (blendmodeCount == 1) {
    blendMode = pass.state.blendModes[0];
  }

  VkClearValue clearValue;
  bool hasClearValue = clearColorCount > 0;
  if (binding.location < clearColorCount) {
    clearValue = pass.state.clearValues[binding.location];
  } else if (clearColorCount == 1) {
    clearValue = pass.state.clearValues[0];
  }

  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  if (hasClearValue) {
    loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  } else if (blendMode.enabled) {
    loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  return AttachmentInfo{.loadOp = loadOp,
                        .storeOp = storeOp,
                        .clearValue = clearValue,
                        .blendMode = blendMode};
}

auto inline GetDescriptorType(const Resource &resource,
                              const ResourceBinding &binding)
    -> VkDescriptorType {

  VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;

  if (resource.type == Type::Texture) {
    if (binding.type == BindingType::Sampler) {
      descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    } else if (binding.type == BindingType::Storage) {
      descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
  } else if (resource.type == Type::Buffer) {
    if (binding.type == BindingType::Uniform) {
      descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    } else if (binding.type == BindingType::Storage) {
      descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
  }

  assert(descriptorType != VK_DESCRIPTOR_TYPE_MAX_ENUM &&
         "Invalid resource binding configuration");

  return descriptorType;
}

[[nodiscard]] auto inline CreateGraphDescriptorPool(GraphicsContext &context,
                                                    RenderGraph &graph)
    -> Error::Error {
  std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts;

  uint32_t totalSets = 0;

  for (const auto &pass : graph.passes) {
    // Count unique sets

    std::unordered_set<uint32_t> uniqueSets;

    for (const auto &binding : pass.resourceBindings) {
      if (binding.type == BindingType::Attachment) {
        continue; // skip attachments
      }

      const auto &resource = graph.resources[binding.resource];

      VkDescriptorType descriptorType = GetDescriptorType(resource, binding);
      descriptorTypeCounts[descriptorType]++;

      uniqueSets.insert(binding.set);
    }

    totalSets += static_cast<uint32_t>(uniqueSets.size());
  }

  constexpr double AllocationMuliplier = 0.1; // 10% extra

  std::vector<VkDescriptorPoolSize> poolSizes;
  for (const auto &typeCount : descriptorTypeCounts) {
    auto type = typeCount.first;
    auto count = typeCount.second;

    count += (std::max)(1U, static_cast<uint32_t>(static_cast<double>(count) *
                                                  AllocationMuliplier));

    poolSizes.push_back(
        VkDescriptorPoolSize{.type = type, .descriptorCount = count});
  }

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = totalSets;

  VkResult result = vkCreateDescriptorPool(context.device, &poolInfo, nullptr,
                                           &graph.descriptorPool);
  if (result != VK_SUCCESS) {
    return Error::FromVkResult(result);
  }

  return Error::Success();
}

auto inline CreatePassDescriptorSetLayouts(GraphicsContext &context,
                                           RenderGraph &graph,
                                           CompiledPass &pass) -> Error::Error {

  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      setBindings;

  for (auto &binding : pass.pass.resourceBindings) {
    if (binding.type == BindingType::Attachment) {
      continue; // skip attachments
    }

    std::cout << "Pass " << pass.pass.handle
              << ": Processing resource binding for resource "
              << binding.resource << " at set " << binding.set << ", binding "
              << binding.binding << "\n";

    const auto &resource = graph.resources[binding.resource];

    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding.binding; // binding in the set
    layoutBinding.descriptorType = GetDescriptorType(resource, binding);
    layoutBinding.descriptorCount = 1;              // not an array
    layoutBinding.stageFlags = VK_SHADER_STAGE_ALL; // adjust as needed
    layoutBinding.pImmutableSamplers = nullptr;

    setBindings[binding.set].push_back(layoutBinding); // add to the correct set
  }

  // Create descriptor set layouts
  for (const auto &setBindingPair : setBindings) {
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount =
        static_cast<uint32_t>(setBindingPair.second.size());
    layoutInfo.pBindings = setBindingPair.second.data();

    VkDescriptorSetLayout descriptorSetLayout = nullptr;
    VkResult result = vkCreateDescriptorSetLayout(
        context.device, &layoutInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
      return Error::FromVkResult(result);
    }

    pass.pass.state.descriptorSetLayouts[setBindingPair.first] =
        descriptorSetLayout;
  }

  return Error::Success();
}

auto inline CreateGraphDescriptorSetLayouts(GraphicsContext &context,
                                            RenderGraph &graph)
    -> Error::Error {
  for (auto &compiledPass : graph.compiledPasses) {
    auto error = CreatePassDescriptorSetLayouts(context, graph, compiledPass);
    if (Error::IsError(error)) {
      return error;
    }
  }

  return Error::Success();
}

auto inline CreatePassDescriptorSets(GraphicsContext &context,
                                     RenderGraph &graph, CompiledPass &pass)
    -> Error::Error {
  std::vector<VkDescriptorSetLayout> layouts;
  layouts.reserve(pass.pass.state.descriptorSetLayouts.size());

  std::cout << "Creating descriptor sets for pass " << pass.pass.handle << "\n";
  std::cout << "Descriptor set layout count: "
            << pass.pass.state.descriptorSetLayouts.size() << "\n";

  if (pass.pass.state.descriptorSetLayouts.empty()) {
    std::cout << "Pass " << pass.pass.handle
              << ": No descriptor set layouts, skipping allocation." << "\n";
    return Error::Success(); // No descriptor sets needed
  }

  for (const auto &setBindingPair : pass.pass.state.descriptorSetLayouts) {
    std::cout << "Pass " << pass.pass.handle
              << ": Using descriptor set layout for set "
              << setBindingPair.first << "\n";
    layouts.emplace_back(setBindingPair.second); // Add all set layouts
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = graph.descriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
  allocInfo.pSetLayouts = layouts.data();

  std::vector<VkDescriptorSet> descriptorSets(layouts.size());

  VkResult result = vkAllocateDescriptorSets(context.device, &allocInfo,
                                             descriptorSets.data());
  if (result != VK_SUCCESS) {
    return Error::FromVkResult(result);
  }

  // Map allocated sets back to their set numbers
  size_t idx = 0;
  for (const auto &setBindingPair : pass.pass.state.descriptorSetLayouts) {
    uint32_t setNumber = setBindingPair.first;
    pass.pass.state.descriptorSets[setNumber] = descriptorSets[idx++];
  }

  return Error::Success();
}

auto inline CreateGraphDescriptorSets(GraphicsContext &context,
                                      RenderGraph &graph) -> Error::Error {
  for (auto &compiledPass : graph.compiledPasses) {
    auto error = CreatePassDescriptorSets(context, graph, compiledPass);
    if (Error::IsError(error)) {
      return error;
    }
  }

  return Error::Success();
}

auto inline ConfigurePassDescriptors(GraphicsContext &context,
                                     RenderGraph &graph, CompiledPass &pass,
                                     const ResourceBinding &binding) -> void {
  // For each pass, configure descriptors for read/write resources
  const auto &resHandle = binding.resource;
  auto &resource = graph.resources[resHandle];

  VkDescriptorImageInfo imageInfo = {};
  VkDescriptorBufferInfo bufferInfo = {};

  if (resource.type == Type::Texture) {
    auto &texture = std::get<Texture::Texture>(resource.info);
    if (binding.type == BindingType::Attachment) {
      return; // Skip raster attachments
    }

    // Binding type allows:
    // Sampled: read-only sampler2D
    // Storage: read-write image2D
    auto layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (binding.type == BindingType::Storage) {
      if (binding.usage == ResourceUsage::ReadOnly) {
        layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      } else if (binding.usage == ResourceUsage::ReadWrite ||
                 binding.usage == ResourceUsage::WriteOnly) {
        layout = VK_IMAGE_LAYOUT_GENERAL;
      }
    } else if (binding.type == BindingType::Sampler) {
      layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    std::cout << "Setting up color attachment at location " << binding.location
              << "\n";
    std::cout << "  Resource lifetime type: "
              << (resource.lifetime == ResourceLifetime::Transient
                      ? "Transient"
                      : "Persistent")
              << "\n";

    imageInfo.imageLayout = layout;
    imageInfo.imageView = texture.view;
    assert(imageInfo.imageView != VK_NULL_HANDLE &&
           "Texture image view is null in descriptor setup");
    imageInfo.sampler = texture.GetSampler(context);
    std::cout << "Configured descriptor for texture resource "
              << resource.handle << " with sampler "
              << (imageInfo.sampler != VK_NULL_HANDLE ? "set" : "null") << "\n";
    auto cache = GetSamplerCache();

    cache[resource.handle] = imageInfo.sampler;

  } else if (resource.type == Type::Buffer) {
    const auto &buffer = std::get<Buffer>(resource.info);

    bufferInfo.offset = 0;
    bufferInfo.range = buffer.sizeInBytes;
    // Buffer would be set during actual execution
    bufferInfo.buffer = VK_NULL_HANDLE;
  }

  VkWriteDescriptorSet writeDesc = {};
  writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDesc.dstBinding = binding.binding;
  writeDesc.dstArrayElement = 0;
  writeDesc.descriptorCount = 1;
  writeDesc.descriptorType = GetDescriptorType(resource, binding);
  writeDesc.pImageInfo = resource.type == Type::Texture ? &imageInfo : nullptr;
  writeDesc.pBufferInfo = resource.type == Type::Buffer ? &bufferInfo : nullptr;
  writeDesc.dstSet = pass.pass.state.descriptorSets.at(binding.set);

  vkUpdateDescriptorSets(context.device, 1, &writeDesc, 0, nullptr);

  pass.pass.state.descriptorWrites.emplace_back(writeDesc);
}

auto inline ConfigureGraphDescriptors(GraphicsContext &context,
                                      RenderGraph &graph) -> void {
  // For each compiled pass, configure descriptors
  for (auto &compiledPass : graph.compiledPasses) {
    for (const auto &binding : compiledPass.pass.resourceBindings) {
      ConfigurePassDescriptors(context, graph, compiledPass, binding);
    }
  }
}

[[nodiscard]] auto inline CreateGraphicsPipeline(GraphicsContext &context,
                                                 RenderGraph &graph,
                                                 CompiledPass &compiledPass)
    -> Error::Error {

  if (compiledPass.pass.state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Create(
        "Cannot create graphics pipeline for non-graphics pass");
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = compiledPass.pass.vertexShader.module;
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = compiledPass.pass.fragmentShader.module;
  shaderStages[1].pName = "main";

  auto vertexformat = Graphics::PredefinedVertexFormats.at(VertexFormats::GUI);

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = vertexformat.Bindings.size();
  vertexInputInfo.pVertexBindingDescriptions = vertexformat.Bindings.data();
  vertexInputInfo.vertexAttributeDescriptionCount =
      vertexformat.Attributes.size();
  vertexInputInfo.pVertexAttributeDescriptions = vertexformat.Attributes.data();
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &compiledPass.pass.state.viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &compiledPass.pass.state.scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0F;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments{};
  uint32_t blendModeCount = 0;

  // Loop over attachments and use GetPassAttachmentInfo to fetch blend modes
  for (const auto &binding : compiledPass.pass.resourceBindings) {
    if (binding.type == BindingType::Attachment) {
      // Allocate blend attachment
      AttachmentInfo attachInfo =
          GetPassAttachmentInfo(compiledPass.pass, binding);

      VkPipelineColorBlendAttachmentState blendAttachment = {};
      blendAttachment.blendEnable =
          attachInfo.blendMode.enabled ? VK_TRUE : VK_FALSE;
      blendAttachment.srcColorBlendFactor =
          attachInfo.blendMode.srcColorBlendFactor;
      ;
      blendAttachment.dstColorBlendFactor =
          attachInfo.blendMode.dstColorBlendFactor;
      blendAttachment.colorBlendOp = attachInfo.blendMode.colorBlendOp;
      blendAttachment.srcAlphaBlendFactor =
          attachInfo.blendMode.srcAlphaBlendFactor;
      blendAttachment.dstAlphaBlendFactor =
          attachInfo.blendMode.dstAlphaBlendFactor;
      blendAttachment.alphaBlendOp = attachInfo.blendMode.alphaBlendOp;
      blendAttachment.colorWriteMask =
          static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
          static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
          static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
          static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT);

      blendAttachments.resize(binding.location + 1);
      blendModeCount++;
      blendAttachments[binding.location] = blendAttachment;
    }
  }

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = blendModeCount;
  colorBlending.pAttachments = blendAttachments.data();

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

  // Count color attachments
  uint32_t colorAttachmentCount = 0;
  bool hasDepthAttachment = false;
  bool hasStencilAttachment = false;
  uint32_t maxLocation = 0;

  for (const auto &binding : compiledPass.pass.resourceBindings) {
    if (binding.type == BindingType::Attachment) {
      auto &resource = graph.resources[binding.resource];
      if (resource.type == Type::Texture) {
        const auto &texture = std::get<Texture::Texture>(resource.info);
        if (Texture::IsDepthTexture(texture.format)) {
          hasDepthAttachment = true;
        } else if (Texture::IsStencilTexture(texture.format)) {
          hasStencilAttachment = true;
        } else {
          colorAttachmentCount++;
          maxLocation =
              (std::max)(maxLocation, static_cast<uint32_t>(binding.location));
        }
      }
    }
  }

  renderingCreateInfo.colorAttachmentCount = colorAttachmentCount;
  std::cout << "Creating graphics pipeline with " << colorAttachmentCount
            << " color attachments."
            << "\n";

  auto formats = std::vector<VkFormat>(maxLocation + 1, VK_FORMAT_UNDEFINED);

  for (const auto &binding : compiledPass.pass.resourceBindings) {
    if (binding.type == BindingType::Attachment) {
      auto &resource = graph.resources[binding.resource];
      const auto &texture = std::get<Texture::Texture>(resource.info);

      formats[binding.location] = texture.format;
      std::cout << "  Using format for attachment at location "
                << binding.location << ": "
                << static_cast<uint32_t>(formats[binding.location]) << "\n";
    }
  }

  renderingCreateInfo.pColorAttachmentFormats = formats.data();

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 0;
  dynamicState.pDynamicStates = nullptr;

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = compiledPass.pass.state.pipelineLayout;
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &renderingCreateInfo;

  auto error = Error::FromVkResult(vkCreateGraphicsPipelines(
      context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
      &compiledPass.pass.state.pipeline));

  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

[[nodiscard]] auto inline CreateComputePipeline(GraphicsContext &context,
                                                RenderGraph &graph,
                                                CompiledPass &compiledPass)
    -> Error::Error {
  if (compiledPass.pass.state.bindPoint != VK_PIPELINE_BIND_POINT_COMPUTE) {
    return Error::Create("Cannot create compute pipeline for non-compute pass");
  }

  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStage.module = compiledPass.pass.computeShader.module;
  shaderStage.pName = "main";

  VkComputePipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage = shaderStage;
  pipelineInfo.layout = compiledPass.pass.state.pipelineLayout;
  auto error = Error::FromVkResult(
      vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
                               nullptr, &compiledPass.pass.state.pipeline));

  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

[[nodiscard]] auto inline CreateGraphPipelines(GraphicsContext &context,
                                               RenderGraph &graph)
    -> Error::Error {
  for (size_t passIndex = 1; passIndex < graph.compiledPasses.size();
       passIndex++) {

    auto descriptorSetLayouts = std::vector<VkDescriptorSetLayout>{};

    for (const auto &setLayoutPair :
         graph.compiledPasses[passIndex].pass.state.descriptorSetLayouts) {
      descriptorSetLayouts.push_back(setLayoutPair.second);
    }

    auto &compiledPass = graph.compiledPasses[passIndex];

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount =
        static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    VkPipelineLayout pipelineLayout = nullptr;
    auto error = Error::FromVkResult(vkCreatePipelineLayout(
        context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    if (Error::IsError(error)) {
      return error;
    }

    compiledPass.pass.state.pipelineLayout = pipelineLayout;
    if (compiledPass.pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      auto error = CreateGraphicsPipeline(context, graph, compiledPass);

      if (Error::IsError(error)) {
        return error;
      }

    } else if (compiledPass.pass.state.bindPoint ==
               VK_PIPELINE_BIND_POINT_COMPUTE) {
      auto error = CreateComputePipeline(context, graph, compiledPass);

      if (Error::IsError(error)) {
        return error;
      }
    } else {
      return Error::Create("Unsupported pipeline bind point");
    }
  }

  return Error::Success();
}

[[nodiscard]] auto inline BuildVirtualMemory(GraphicsContext &context,
                                             RenderGraph &graph)
    -> Error::Error {
  // Loop over compiled resource timeline and allocate/deallocate as needed

  graph.virtualAllocations.clear();
  for (const auto &entry : graph.compiledResources) {
    const auto &resource = graph.resources[entry.resourceHandle];

    if (entry.type == ResourceTimelineEntryType::Allocate) {
      VkDeviceSize alignment = 1;
      VkDeviceSize size = 0;

      AllocationInfo allocationInfo = {};
      allocationInfo.handle = resource.handle;

      if (resource.type == Type::Texture) {
        const auto &tex = std::get<Texture::Texture>(resource.info);

        // Heuristic size (not actual alloc size)
        auto texels = Texture::GetTexelCount(tex.size, tex.mipmapcount);
        texels *= tex.arrayLayers;
        allocationInfo.size = texels * Texture::GetFormatSize(tex.format);

        allocationInfo.alignment = QueryMemoryAlignmentOfTexture(context, tex);
      } else {
        const auto &buf = std::get<Buffer>(resource.info);
        allocationInfo.size = buf.size;
        allocationInfo.alignment = QueryMemoryAlignmentOfBuffer(context, buf);
      }

      auto error = AllocateResourceInBlocks(context, graph, allocationInfo);
      if (Error::IsError(error)) {
        return error;
      }
    } else if (entry.type == ResourceTimelineEntryType::Deallocate) {
      DeallocateResourceInBlocks(graph, resource.handle);
    }
  }

  return Error::Success();
}

[[nodiscard]] auto inline AllocateBlockMemory(GraphicsContext &context,
                                              RenderGraph &graph)
    -> Error::Error {
  // For each memory block, allocate a VkDeviceMemory

  for (auto &block : graph.memoryBlocks) {

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = block.size;

    // For simplicity, use a generic memory type index
    // In a real implementation, this should be based on resource requirements
    allocInfo.memoryTypeIndex = 0;

    VkDeviceMemory memory = nullptr;
    VkResult result =
        vkAllocateMemory(context.device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
      return Error::Create("Failed to allocate device memory for render graph");
    }

    block.memory = memory;
  }

  return Error::Success();
}

auto inline AllocateResourceMemory(GraphicsContext &context, RenderGraph &graph,
                                   const ResourceHandle handle)
    -> Error::Error {
  auto allocationIterator = graph.virtualAllocations.find(handle);
  bool found = allocationIterator != graph.virtualAllocations.end();

  if (!found) {
    return Error::Create("Resource allocation for resource: [" +
                         std::to_string(handle) +
                         "] not found in render graph.");
  }

  auto allocation = allocationIterator->second;

  auto &block = graph.memoryBlocks[allocation.blockIndex];
  auto &resource = graph.resources[handle];

  if (resource.lifetime == ResourceLifetime::Persistent) {
    return Error::Create(
        "Cannot allocate memory for persistent resource in render graph.");
  }

  if (resource.type == Type::Texture) {
    auto &texture = std::get<Texture::Texture>(resource.info);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = texture.format;
    imageInfo.extent = texture.size;
    imageInfo.mipLevels = texture.mipmapcount;
    imageInfo.arrayLayers = texture.arrayLayers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = texture.usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto result = Error::FromVkResult(
        vkCreateImage(context.device, &imageInfo, nullptr, &texture.image));

    if (Error::IsError(result)) {
      return result;
    }

    result = Error::FromVkResult(vkBindImageMemory(
        context.device, texture.image, block.memory, allocation.offset));
    if (Error::IsError(result)) {
      return result;
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = texture.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = texture.mipmapcount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = texture.arrayLayers;

    result = Error::FromVkResult(
        vkCreateImageView(context.device, &viewInfo, nullptr, &texture.view));

    if (Error::IsError(result)) {
      return result;
    }
  } else if (resource.type == Type::Buffer) {
    auto &buffer = std::get<Buffer>(resource.info);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buffer.size;
    bufferInfo.usage = buffer.usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide
    allocInfo.requiredFlags = buffer.properties;

    VkResult result =
        vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                        &buffer.handle, &buffer.memory, nullptr);
  }

  return Error::Success();
}

auto inline ValidateResources(const RenderGraph &graph) -> Error::Error {
  for (const auto &resource : graph.resources) {
    if (resource.type == Type::Texture) {
      const auto &texture = std::get<Texture::Texture>(resource.info);
      if (texture.image == VK_NULL_HANDLE || texture.view == VK_NULL_HANDLE) {
        return Error::Create("Texture resource not properly allocated");
      }
    } else if (resource.type == Type::Buffer) {
      const auto &buffer = std::get<Buffer>(resource.info);
      if (buffer.handle == VK_NULL_HANDLE) {
        return Error::Create("Buffer resource not properly allocated");
      }
    }
  }

  // check if transient resource is written to but never read: warning
  // check if transient resource is read from but never written: warning

  std::unordered_set<ResourceHandle> writtenResources;
  std::unordered_set<ResourceHandle> readResources;

  for (const auto &compiledPass : graph.compiledPasses) {
    for (const auto &resHandle :
         compiledPass.pass.GetResources(AccessType::Write)) {
      writtenResources.insert(resHandle);
    }
    for (const auto &resHandle :
         compiledPass.pass.GetResources(AccessType::Read)) {
      readResources.insert(resHandle);
    }
  }

  for (const auto &resource : graph.resources) {
    if (resource.lifetime == ResourceLifetime::Transient) {
      bool isWritten = writtenResources.contains(resource.handle);
      bool isRead = readResources.contains(resource.handle);

      if (isWritten && !isRead) {
        std::cout << "Warning: Transient resource " << resource.handle
                  << " is written to but never read." << "\n";
      } else if (!isWritten && isRead) {
        std::cout << "Warning: Transient resource " << resource.handle
                  << " is read from but never written." << "\n";
      }
    }
  }

  return Error::Success();
}

[[nodiscard]]

auto inline AllocateGraphResourceMemory(GraphicsContext &context,
                                        RenderGraph &graph) -> Error::Error {
  std::unordered_set<ResourceHandle> usedResources;

  for (const auto &compiledPass : graph.compiledPasses) {
    for (const auto &resHandle :
         compiledPass.pass.GetResources(static_cast<AccessType>(
             static_cast<uint32_t>(AccessType::Read) |
             static_cast<uint32_t>(AccessType::Write)))) {
      auto &resource = graph.resources[resHandle];
      if (resource.lifetime == ResourceLifetime::Persistent) {
        continue; // Skip persistent resources
      }

      usedResources.insert(resHandle);
      std::cout << "Resource " << resHandle << " is used in pass "
                << compiledPass.pass.handle << "\n";
    }
  }

  for (const auto &resHandle : usedResources) {
    auto error = AllocateResourceMemory(context, graph, resHandle);
    if (Error::IsError(error)) {
      return error;
    }
  }

  return Error::Success();
}

[[nodiscard]] auto Compile(GraphicsContext &context, RenderGraph &graph)
    -> Error::Error {
  // For each resource, calculate cost

  std::cout << "Compiling render graph..." << "\n";

  for (auto &resource : graph.resources) {
    if (resource.type == Type::Texture) {
      auto &texture = std::get<Texture::Texture>(resource.info);
      resource.cost =
          Texture::GetTexelCount(texture.size, texture.mipmapcount) *
          Texture::GetFormatSize(texture.format);

      resource.cost *= texture.arrayLayers;
    } else if (resource.type == Type::Buffer) {
      auto &buffer = std::get<Buffer>(resource.info);
      resource.cost = static_cast<uint32_t>(buffer.size);
    }
  }

  BuildGraph(graph);

  BuildVirtualRoot(graph);

  // Schedule nodes based on heuristic
  ScheduleNodes(graph);

  CalculateResourceLifetimes(graph);

  auto validationResult = ValidateCompiledGraph(graph);
  if (Error::IsError(validationResult)) {
    return validationResult.error();
  }

  CompileResourceTimeline(graph);
  auto error = BuildVirtualMemory(context, graph);
  if (Error::IsError(error)) {
    return error;
  }
  error = AllocateBlockMemory(context, graph);
  if (Error::IsError(error)) {
    return error;
  }

  std::cout << "Allocated " << graph.memoryBlocks.size()
            << " memory blocks for render graph." << "\n";

  error = AllocateGraphResourceMemory(context, graph);
  if (Error::IsError(error)) {
    return error;
  }

  std::cout << "Allocated memory for render graph resources." << "\n";

  error = ValidateResources(graph);
  if (Error::IsError(validationResult)) {
    return validationResult.error();
  }

  error = CreateGraphDescriptorPool(context, graph);

  if (Error::IsError(error)) {
    return error;
  }

  std::cout << "Created descriptor pool for render graph." << "\n";

  error = CreateGraphDescriptorSetLayouts(context, graph);

  if (Error::IsError(error)) {
    return error;
  }

  std::cout << "Created descriptor set layouts for render graph passes."
            << "\n";

  error = CreateGraphDescriptorSets(context, graph);

  if (Error::IsError(error)) {
    return error;
  }

  std::cout << "Allocated descriptor sets for render graph passes." << "\n";

  ConfigureGraphDescriptors(context, graph);

  std::cout << "Configured descriptors for render graph passes." << "\n";
  error = CreateGraphPipelines(context, graph);
  if (Error::IsError(error)) {
    return error;
  }

  std::cout << "Created pipelines for render graph passes." << "\n";

  return Error::Success();
}

auto BeginPassRendering(GraphicsContext &context, RenderGraph &graph,
                        VkCommandBuffer commandBuffer,
                        const CompiledPass &compiledPass) -> void {
  // Calculate max binding index for color attachments
  uint32_t maxColorAttachmentIndex = 0;
  for (const auto &binding : compiledPass.pass.resourceBindings) {
    if (binding.type == BindingType::Attachment) {
      maxColorAttachmentIndex =
          (std::max)(binding.location, maxColorAttachmentIndex);
    }
  }

  std::vector<VkRenderingAttachmentInfo> colorAttachments;
  colorAttachments.resize(maxColorAttachmentIndex + 1);

  std::cout << "Max color attachment index: " << maxColorAttachmentIndex
            << "\n";

  bool hasDepthAttachment = false;
  bool hasStencilAttachment = false;

  for (const auto &binding : compiledPass.pass.resourceBindings) {
    if (binding.type == BindingType::Attachment) {
      const auto &resource = graph.resources[binding.resource];
      AttachmentInfo attachInfo =
          GetPassAttachmentInfo(compiledPass.pass, binding);

      Texture::Texture texture = std::get<Texture::Texture>(resource.info);

      if (Texture::IsDepthTexture(texture.format)) {
        hasDepthAttachment = true;
        continue;
      }

      if (Texture::IsStencilTexture(texture.format)) {
        hasStencilAttachment = true;
        continue;
      }

      std::cout << "Setting up color attachment at location "
                << binding.location << "\n";
      std::cout << "  Resource lifetime type: "
                << (resource.lifetime == ResourceLifetime::Transient
                        ? "Transient"
                        : "Persistent")
                << "\n";

      VkRenderingAttachmentInfo colorAttach = {};
      colorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      colorAttach.loadOp = attachInfo.loadOp;
      colorAttach.storeOp = attachInfo.storeOp;
      colorAttach.clearValue = attachInfo.clearValue;

      colorAttach.imageView = texture.view;
      assert(colorAttach.imageView != VK_NULL_HANDLE &&
             "Invalid image view for color attachment");
      colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      colorAttach.resolveMode = VK_RESOLVE_MODE_NONE; // For multi-sampling
      colorAttach.resolveImageView = VK_NULL_HANDLE;
      colorAttach.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      colorAttachments[binding.location] = colorAttach;
    }
  }

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.offset = {
      .x = static_cast<int32_t>(compiledPass.pass.state.viewport.x),
      .y = static_cast<int32_t>(compiledPass.pass.state.viewport.y)};
  renderingInfo.renderArea.extent = {
      .width = static_cast<uint32_t>(compiledPass.pass.state.viewport.width),
      .height = static_cast<uint32_t>(compiledPass.pass.state.viewport.height)};
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  renderingInfo.colorAttachmentCount =
      static_cast<uint32_t>(colorAttachments.size());
  renderingInfo.pColorAttachments = colorAttachments.data();
  renderingInfo.pDepthAttachment = nullptr;
  renderingInfo.pStencilAttachment = nullptr;

  std::cout << "Beginning rendering, color attachments: "
            << renderingInfo.colorAttachmentCount << "\n";

  vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

auto Execute(GraphicsContext &context, RenderGraph &graph,
             VkCommandBuffer commandBuffer) -> void {
  // For each compiled pass, record commands
  for (size_t passIndex = 1; passIndex < graph.compiledPasses.size();
       passIndex++) {
    auto &compiledPass = graph.compiledPasses[passIndex];

    if (compiledPass.pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      BeginPassRendering(context, graph, commandBuffer, compiledPass);
    } else {
      assert(compiledPass.pass.state.bindPoint ==
             VK_PIPELINE_BIND_POINT_COMPUTE);
    }

    // Todo, loop over all samplers, and check against the sampler cache to
    // see if we have created a new sampler and need to bind the new one

    /*
    auto cache = GetSamplerCache();
    auto samplerIterator = cache.find(resource.handle);
    if (imageInfo.sampler != samplerIterator->second ||
        samplerIterator == cache.end()) {
      cache[resource.handle] = imageInfo.sampler;
    }
    */

    // Bind pipeline
    vkCmdBindPipeline(commandBuffer, compiledPass.pass.state.bindPoint,
                      compiledPass.pass.state.pipeline);

    if (compiledPass.pass.state.descriptorSets.size() > 0) {
      std::vector<VkDescriptorSet> descriptorSets;
      descriptorSets.reserve(compiledPass.pass.state.descriptorSets.size());

      for (const auto &setPair : compiledPass.pass.state.descriptorSets) {
        descriptorSets.emplace_back(setPair.second);
      }

      vkCmdBindDescriptorSets(commandBuffer, compiledPass.pass.state.bindPoint,
                              compiledPass.pass.state.pipelineLayout, 0,
                              descriptorSets.size(), descriptorSets.data(), 0,
                              nullptr);
    }

    std::cout << "Executing pass " << compiledPass.pass.handle << "\n";

    compiledPass.pass.executeFunction(commandBuffer, context, graph,
                                      compiledPass);

    std::cout << "Executed pass " << compiledPass.pass.handle << "\n";

    if (compiledPass.pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      vkCmdEndRendering(commandBuffer);
    }
  }

  std::cout << "Finished executing render graph." << "\n";
}

auto AddTexture(RenderGraph &graph, const TextureDescriptor &descriptor)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = descriptor.lifetime;
  resource.type = Type::Texture;

  Texture::Texture texture = {};

  bool volumeTexture = descriptor.type == Texture::TextureType::VOLUME;
  uint32_t depth = volumeTexture ? descriptor.depthOrLayers : 1U;
  uint32_t layers = volumeTexture ? 1U : descriptor.depthOrLayers;

  texture.format = descriptor.format;
  texture.size = {
      .width = descriptor.width, .height = descriptor.height, .depth = depth};
  texture.mipmapcount = descriptor.mipLevels;
  texture.arrayLayers = layers;
  texture.usage = descriptor.usage;
  texture.type = descriptor.type;
  texture.samplerDirty = true;

  resource.info = texture;
  graph.resources.emplace_back(resource);

  std::cout << "Added texture resource with handle " << resource.handle << "\n";
  std::cout << "  Usage flags: " << descriptor.usage << "\n";

  return resource.handle;
}

auto AddBuffer(RenderGraph &graph, BufferDescriptor descriptor)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = descriptor.lifetime;
  resource.type = Type::Buffer;

  Buffer buffer = {};

  buffer.size = descriptor.size;
  buffer.usage = descriptor.usage;
  buffer.properties = descriptor.memory;

  resource.info = buffer;
  graph.resources.emplace_back(resource);

  return resource.handle;
}

auto ImportTexture(RenderGraph &graph, const Graphics::Texture::Texture texture,
                   VkImageLayout initialLayout, // NOLINT
                   VkImageLayout finalLayout) -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = ResourceLifetime::Persistent;
  resource.type = Type::Texture;
  resource.info = texture;

  assert(texture.view != VK_NULL_HANDLE &&
         "Imported texture must have a valid image view");

  graph.resources.emplace_back(resource);

  std::cout << "Imported texture resource with handle " << resource.handle
            << "\n";
  std::cout << "  Usage flags: " << texture.usage << "\n";

  return resource.handle;
}

auto ImportBuffer(RenderGraph &graph, const Graphics::Buffer &buffer)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = ResourceLifetime::Persistent;
  resource.type = Type::Buffer;

  resource.info = buffer;
  graph.resources.emplace_back(resource);

  return resource.handle;
}

} // namespace Graphics::Rendergraph