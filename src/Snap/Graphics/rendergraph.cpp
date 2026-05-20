#include "rendergraph.hpp"
#include "Graphics/allocations.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "texture.hpp"
#include <unordered_map>

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <unordered_set>

namespace Graphics::Rendergraph {
using std::max;
using std::sort;

auto SetupRenderPassResources(RenderGraph &graph, RenderPass &pass,
                              const RenderPassDescriptor &descriptor) {
  for (const auto &handle : descriptor.resources) {
    auto &resource = graph.resources[handle];
    // Loop through descriptor.resourceBindings, find matching resource handle
    ResourceBinding const *binding = nullptr;
    for (const auto &currentBinding : descriptor.resourceBindings) {
      if (currentBinding.resource == handle) {
        binding = &currentBinding;
        break;
      }
    }

    assert(binding != nullptr &&
           "Resource binding not found for resource in render pass descriptor");

    if (binding->usage == ResourceUsage::ReadOnly) {
      pass.readResources.emplace_back(handle);
    } else if (binding->usage == ResourceUsage::WriteOnly) {
      pass.writeResources.emplace_back(handle);
    } else if (binding->usage == ResourceUsage::ReadWrite) {
      pass.readwriteResources.emplace_back(handle);
    }
  }
}

auto AddRenderPass(RenderGraph &graph, const RenderPassDescriptor &descriptor)
    -> ResourceHandle {
  RenderPass pass = {};

  pass.handle = static_cast<ResourceHandle>(graph.passes.size());

  SetupRenderPassResources(graph, pass, descriptor);

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

  pass.shader = descriptor.shader;
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
  assert((pass.shader.get() != nullptr) && "shader must be set.");

  auto &shader = *pass.shader;
  assert(shader.module != VK_NULL_HANDLE && "Shader module must be present");

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

  PrintDebug("Scheduled {} passes.", graph.compiledPasses.size());
  PrintDebug("----------------------------");
  // Skip virtual root
  std::string passOrder;
  for (int i = 1; i < graph.compiledPasses.size(); i++) {
    passOrder += std::to_string(i) + " (" +
                 std::to_string(graph.compiledPasses[i].pass.handle) + ")";
    if (i != graph.compiledPasses.size() - 1) {
      passOrder += " -> ";
    }
  }
  PrintDebug("{}\n----------------------------", passOrder);
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
    -> Result<bool> {
  // For now just check if the last pass does not write any resources
  // It can write persistent resources since they live beyond the graph
  // execution (e.g. swapchain images, or other long-lived targets)

  const auto &lastPass = graph.compiledPasses.back().pass;

  for (const auto &resHandle : lastPass.GetResources(AccessType::Write)) {
    const auto &resource = graph.resources[resHandle];

    if (resource.lifetime == ResourceLifetime::Transient) {
      return Error::Unexpected(
          "Render graph validation failed: last pass writes "
          "resource " +
          std::to_string(resHandle));
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
    -> Error {
  MemoryBlock block = {};
  block.size = size == 0 ? graph.memoryBlockSize : size;
  block.offset = 0;

  VmaVirtualBlockCreateInfo blockCreateInfo = {};
  blockCreateInfo.size = block.size;

  Error error = Error::Create(
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
    -> Error {

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

      PrintDebug("Allocated resource {} in existing memory block {} of size {} "
                 "at {}\n",
                 info.handle, blockIndex, info.size, offset);

      return Error::Success(); // success
    }
  }

  // Create larger block if needed
  uint32_t allocationSize = (std::max)(info.size, graph.memoryBlockSize);

  CHECK_ERR(ReserveBlock(context, graph, allocationSize));

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
    PrintDebug("Allocated resource {} in new memory block {} of size {} "
               "at {}\n",
               info.handle, graph.memoryBlocks.size() - 1, allocationSize,
               offset);

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
                                          const Ref<Texture> &texture)
    -> VkDeviceSize {
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = texture->format;
  imageInfo.extent = texture->size;
  imageInfo.mipLevels = texture->mipmapcount;
  imageInfo.arrayLayers = texture->arrayLayers;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = texture->usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImage tempImage = nullptr;
  VkResult result = vkCreateImage(context.device, &imageInfo,
                                  GetAllocationCallbacks(), &tempImage);
  if (result != VK_SUCCESS) {
    return 0; // failed to create image
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context.device, tempImage, &memRequirements);

  vkDestroyImage(context.device, tempImage, GetAllocationCallbacks());

  return memRequirements.alignment;
}

auto inline QueryMemoryAlignmentOfBuffer(GraphicsContext &context,
                                         const Ref<Buffer> &buffer)
    -> VkDeviceSize {
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = buffer->size;
  bufferInfo.usage = buffer->usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkBuffer tempBuffer = nullptr;
  VkResult result = vkCreateBuffer(context.device, &bufferInfo,
                                   GetAllocationCallbacks(), &tempBuffer);
  if (result != VK_SUCCESS) {
    return 0; // failed to create buffer
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(context.device, tempBuffer, &memRequirements);

  vkDestroyBuffer(context.device, tempBuffer, GetAllocationCallbacks());

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

  PrintDebug("Compiled resource timeline with {} entries.",
             graph.compiledResources.size());
  PrintDebug("----------------------------");

  auto lastPassHandle = static_cast<ResourceHandle>(-2);

  for (const auto &entry : graph.compiledResources) {
    if (entry.passHandle != lastPassHandle) {
      PrintDebug("Pass {}:", entry.passHandle);
      lastPassHandle = entry.passHandle;
    }
    PrintDebug("  {}: {}",
               (entry.type == ResourceTimelineEntryType::Allocate
                    ? "Allocate"
                    : "Deallocate"),
               entry.resourceHandle);
  }
  PrintDebug("----------------------------");
}

struct AttachmentInfo {
  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkClearValue clearValue{};
  VkPipelineColorBlendAttachmentState blendMode = DefaultBlendMode;
};

auto inline GetPassAttachmentInfo(const RenderPass &pass,
                                  const ResourceBinding &binding)
    -> AttachmentInfo {
  size_t blendmodeCount = pass.state.blendModes.size();
  size_t clearColorCount = pass.state.clearValues.size();

  VkPipelineColorBlendAttachmentState blendMode =
      DefaultBlendMode; // Default blend mode: No blending, disabled.

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
  } else if (blendMode.blendEnable != 0U) {
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
    -> Error {
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

  if (totalSets == 0) {
    PrintDebug(
        "No descriptor sets needed, skipping descriptor pool creation.\n");
    return Error::Success();
  }

  constexpr double AllocationMuliplier = 0.1; // 10% extra

  std::vector<VkDescriptorPoolSize> poolSizes;
  for (const auto &typeCount : descriptorTypeCounts) {
    auto type = typeCount.first;
    auto count = typeCount.second;

    count += (std::max)(1U, static_cast<uint32_t>(static_cast<double>(count) *
                                                  AllocationMuliplier));

    poolSizes.emplace_back(
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
    return Error::Create(result);
  }

  return Error::Success();
}

// Determine the appropriate image layout for a resource binding; NOLINTNEXTLINE
auto inline GetImageBindingLayout(const RenderGraph &graph,
                                  const ResourceBinding &binding)
    -> VkImageLayout {
  const auto &resource = graph.resources[binding.resource];
  const auto &texture = std::get<Ref<Texture>>(resource.info);

  bool isDepthTexture = Image::IsDepthTexture(texture->format);
  bool isStencilTexture = Image::IsStencilTexture(texture->format);
  bool isDepthStencilTexture = isDepthTexture && isStencilTexture;

  // === STORAGES === //
  if (binding.type == BindingType::Storage) {
    if (binding.usage == ResourceUsage::ReadOnly) {
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_GENERAL;
  }

  // === SAMPLERS === //
  if (binding.type == BindingType::Sampler) {
    if (isDepthStencilTexture) {
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
    if (isDepthTexture) {
      return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    }
    if (isStencilTexture) {
      return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  // === ATTACHMENTS === //
  if (binding.type == BindingType::Attachment) {

    // == READONLY ATTACHMENTS == //
    if (binding.usage == ResourceUsage::ReadOnly) {
      if (isDepthStencilTexture) {
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      }
      if (isDepthTexture) {
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
      }
      if (isStencilTexture) {
        return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
      }
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // == READ & READWRITE ATTACHMENTS == //
    if (isDepthStencilTexture) {
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    if (isDepthTexture) {
      return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    }
    if (isStencilTexture) {
      return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  return VK_IMAGE_LAYOUT_UNDEFINED; // Fallback
}

auto inline GetImageAccessFlags(const RenderGraph &graph,
                                const ResourceBinding &binding)
    -> VkAccessFlags {
  const auto &resource = graph.resources[binding.resource];

  // === STORAGES === //
  if (binding.type == BindingType::Storage) {
    if (binding.usage == ResourceUsage::ReadOnly) {
      return VK_ACCESS_SHADER_READ_BIT;
    }
    return static_cast<uint32_t>(VK_ACCESS_SHADER_READ_BIT) |
           static_cast<uint32_t>(VK_ACCESS_SHADER_WRITE_BIT);
  }

  // === SAMPLERS === //
  if (binding.type == BindingType::Sampler) {
    return VK_ACCESS_SHADER_READ_BIT;
  }

  // === ATTACHMENTS === //
  if (binding.type == BindingType::Attachment) {

    // == READONLY ATTACHMENTS == //
    if (binding.usage == ResourceUsage::ReadOnly) {
      return VK_ACCESS_SHADER_READ_BIT; // TODO:
                                        // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
                                        // for subpasses
    }

    // == READ & READWRITE ATTACHMENTS == //
    return static_cast<uint32_t>(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT) |
           static_cast<uint32_t>(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  }

  return 0; // Fallback
}

auto inline GetBufferAccessFlags(const RenderGraph &graph,
                                 const ResourceBinding &binding)
    -> VkAccessFlags {
  const auto &resource = graph.resources[binding.resource];

  // === STORAGES === //
  if (binding.type == BindingType::Storage) {
    if (binding.usage == ResourceUsage::ReadOnly) {
      return VK_ACCESS_SHADER_READ_BIT;
    }
    return static_cast<uint32_t>(VK_ACCESS_SHADER_READ_BIT) |
           static_cast<uint32_t>(VK_ACCESS_SHADER_WRITE_BIT);
  }

  // === UNIFORM BUFFER OBJECTS === //
  if (binding.type == BindingType::Uniform) {
    return VK_ACCESS_UNIFORM_READ_BIT;
  }

  return 0; // Fallback
}

// Configure layouts for all passes in the graph
// To be used to create the transitions for resources later on
auto inline CreateGraphLayoutStates(GraphicsContext &context,
                                    RenderGraph &graph) -> void {
  for (auto &compiledPass : graph.compiledPasses) {
    for (const auto &binding : compiledPass.pass.resourceBindings) {
      const auto &resource = graph.resources[binding.resource];

      if (resource.type == Type::Texture) {
        const auto &texture = std::get<Ref<Texture>>(resource.info);

        auto layout = GetImageBindingLayout(graph, binding);

        auto stages =
            compiledPass.pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
                ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
                : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        auto access = GetImageAccessFlags(graph, binding);

        LayoutState layoutState = {
            .layout = layout,
            .stages = static_cast<VkPipelineStageFlags>(stages),
            .access = access,
        };

        compiledPass.resourceLayouts[resource.handle] = layoutState;

        PrintDebug("Pass {}: Resource {} layout set to {}",
                   compiledPass.pass.handle, resource.handle,
                   static_cast<uint32_t>(layout));
      } else if (resource.type == Type::Buffer) {
        auto layout = VK_IMAGE_LAYOUT_UNDEFINED; // Buffers don't have layouts

        auto stages =
            compiledPass.pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS
                ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
                : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        auto access = GetBufferAccessFlags(graph, binding);

        LayoutState layoutState = {
            .layout = layout,
            .stages = static_cast<VkPipelineStageFlags>(stages),
            .access = access,
        };

        compiledPass.resourceLayouts[resource.handle] = layoutState;
      }
    }
  }
}

auto inline CreateGraphResourceTransitions(GraphicsContext &context,
                                           RenderGraph &graph) -> void {

  // We will loop through the timeline and create transitions when layouts
  // change, currentLayouts will track the last known layout of each resource
  // Note, not a reference, we want a copy to track changes
  auto currentLayouts = graph.initialResourceLayouts;

  // For each resource, create transitions based on usage in passes
  for (auto &compiledPass : graph.compiledPasses) {
    for (const auto &binding : compiledPass.pass.resourceBindings) {
      const auto &resource = graph.resources[binding.resource];

      // Create transitions only for textures
      if (resource.type == Type::Texture) {
        const auto &texture = std::get<Ref<Texture>>(resource.info);

        auto currentLayoutIterator = currentLayouts.find(resource.handle);
        LayoutState oldLayoutState = {
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .access = 0,
        };

        if (currentLayoutIterator != currentLayouts.end()) {
          oldLayoutState = currentLayoutIterator->second;
        }

        LayoutState newLayoutState =
            compiledPass.resourceLayouts.at(resource.handle);

        PrintDebug("Pass {}: Resource {} old layout: {}, new layout: {}",
                   compiledPass.pass.handle, resource.handle,
                   static_cast<uint32_t>(oldLayoutState.layout),
                   static_cast<uint32_t>(newLayoutState.layout));

        // If layout has changed, create a transition
        if (oldLayoutState.layout != newLayoutState.layout) {
          LayoutUpdate transition = {
              .resource = resource.handle,
              .oldState = oldLayoutState,
              .newState = newLayoutState,
          };

          compiledPass.layoutUpdates.emplace_back(transition);

          // Update current layout
          currentLayouts[resource.handle] = newLayoutState;
        } else {
          PrintDebug("Pass {}: No layout change for resource {}, skipping "
                     "transition.\n",
                     compiledPass.pass.handle, resource.handle);
        }
      }
    }
  }

  for (const auto &wantedLayout : graph.finalResourceLayouts) {
    const auto &resourceHandle = wantedLayout.first;
    const auto &desiredLayoutState = wantedLayout.second;

    auto currentLayoutIterator = currentLayouts.find(resourceHandle);
    if (currentLayoutIterator == currentLayouts.end()) {
      continue; // not found
    }

    LayoutState currentLayoutState = currentLayoutIterator->second;

    if (currentLayoutState.layout != desiredLayoutState.layout) {
      PrintDebug("Creating final layout transition for resource {}",
                 resourceHandle);

      const auto &resource = graph.resources[resourceHandle];
      const auto &texture = std::get<Ref<Texture>>(resource.info);

      VkImageMemoryBarrier barrier = {};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = currentLayoutState.layout;
      barrier.newLayout = desiredLayoutState.layout;
      barrier.srcAccessMask = currentLayoutState.access;
      barrier.dstAccessMask = desiredLayoutState.access;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = texture->image;
      barrier.subresourceRange.aspectMask =
          Image::GetTextureAspectFlags(texture->format);
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = texture->mipmapcount;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = texture->arrayLayers;

      graph.postGraphUpdates.emplace_back(barrier);
    }
  }
}

auto inline TransitionImageLayouts(GraphicsContext &context, RenderGraph &graph,
                                   CompiledPass &compiledPass) -> Error {
  for (const auto &layoutUpdate : compiledPass.layoutUpdates) {
    const auto &resource = graph.resources[layoutUpdate.resource];
    const auto &texture = std::get<Ref<Texture>>(resource.info);

    auto result = texture->TransitionLayout(
        context, layoutUpdate.newState.layout, layoutUpdate.oldState.stages,
        layoutUpdate.newState.stages, layoutUpdate.oldState.access,
        layoutUpdate.newState.access);
    if (Error::IsError(result)) {
      return result;
    }
  }

  return Error::Success();
}

auto inline ApplyPassBarriers(VkCommandBuffer commandBuffer,
                              const CompiledPass &compiledPass) -> void {
  if (compiledPass.imageBarriers.empty()) {
    return; // No barriers to apply
  }

  vkCmdPipelineBarrier(commandBuffer,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // src stage
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dst stage
                       0,                                  // dependency flags
                       0, nullptr,                         // memory barriers
                       0, nullptr,                         // buffer barriers
                       static_cast<uint32_t>(compiledPass.imageBarriers.size()),
                       compiledPass.imageBarriers.data() // image barriers
  );
}

[[nodiscard]] auto inline BuildVirtualMemory(GraphicsContext &context,
                                             RenderGraph &graph) -> Error {
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
        const auto &tex = std::get<Ref<Texture>>(resource.info);

        // Heuristic size (not actual alloc size)
        auto texels = Image::GetTexelCount(tex->size, tex->mipmapcount);
        texels *= tex->arrayLayers;
        allocationInfo.size = texels * Format::GetSize(tex->format);

        allocationInfo.alignment = QueryMemoryAlignmentOfTexture(context, tex);
      } else {
        const auto &buf = std::get<Ref<Buffer>>(resource.info);
        allocationInfo.size = buf->size;
        allocationInfo.alignment = QueryMemoryAlignmentOfBuffer(context, buf);
      }

      CHECK_ERR(AllocateResourceInBlocks(context, graph, allocationInfo));
    } else if (entry.type == ResourceTimelineEntryType::Deallocate) {
      DeallocateResourceInBlocks(graph, resource.handle);
    }
  }

  return Error::Success();
}

[[nodiscard]] auto inline AllocateBlockMemory(GraphicsContext &context,
                                              RenderGraph &graph) -> Error {
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
                                   const ResourceHandle handle) -> Error {
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
    auto &texture = std::get<Ref<Texture>>(resource.info);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = texture->format;
    imageInfo.extent = texture->size;
    imageInfo.mipLevels = texture->mipmapcount;
    imageInfo.arrayLayers = texture->arrayLayers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = texture->usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    CHECK_ERR(Error::Create(
        vkCreateImage(context.device, &imageInfo, nullptr, &texture->image)));

    CHECK_ERR(Error::Create(vkBindImageMemory(
        context.device, texture->image, block.memory, allocation.offset)));

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = texture->format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = texture->mipmapcount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = texture->arrayLayers;

    CHECK_ERR(Error::Create(vkCreateImageView(context.device, &viewInfo,
                                              nullptr, &(texture->view))));
  } else if (resource.type == Type::Buffer) {
    auto &buffer = std::get<Ref<Buffer>>(resource.info);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buffer->size;
    bufferInfo.usage = buffer->usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // Let VMA decide
    allocInfo.requiredFlags = buffer->properties;

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.vmaAllocator);

      VkResult result =
          vmaCreateBuffer(context.vmaAllocator, &bufferInfo, &allocInfo,
                          &buffer->handle, &buffer->memory, nullptr);
    }
  }

  return Error::Success();
}

auto inline ValidateResources(const RenderGraph &graph) -> Error {
  for (const auto &resource : graph.resources) {
    if (resource.type == Type::Texture) {
      const auto &texture = std::get<Ref<Texture>>(resource.info);
      if (texture->image == VK_NULL_HANDLE || texture->view == VK_NULL_HANDLE) {
        return Error::Create("Texture resource not properly allocated");
      }
    } else if (resource.type == Type::Buffer) {
      const auto &buffer = std::get<Ref<Buffer>>(resource.info);
      if (buffer->handle == VK_NULL_HANDLE) {
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
        PrintWarning("Transient resource {} is written to but never read.\n",
                     resource.handle);
      } else if (!isWritten && isRead) {
        PrintWarning("Transient resource {} is read from but never written.\n",
                     resource.handle);
      }
    }
  }

  return Error::Success();
}

[[nodiscard]]

auto inline AllocateGraphResourceMemory(GraphicsContext &context,
                                        RenderGraph &graph) -> Error {
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
      PrintDebug("Resource {} is used in pass {}", resHandle,
                 compiledPass.pass.handle);
    }
  }

  for (const auto &resHandle : usedResources) {
    CHECK_ERR(AllocateResourceMemory(context, graph, resHandle));
  }

  return Error::Success();
}

[[nodiscard]] auto Compile(GraphicsContext &context, RenderGraph &graph)
    -> Error {
  // For each resource, calculate cost

  PrintDebug("Compiling render graph...");

  for (auto &resource : graph.resources) {
    if (resource.type == Type::Texture) {
      auto &texture = std::get<Ref<Texture>>(resource.info);
      resource.cost =
          Image::GetTexelCount(texture->size, texture->mipmapcount) *
          Format::GetSize(texture->format);

      resource.cost *= texture->arrayLayers;
    } else if (resource.type == Type::Buffer) {
      auto &buffer = std::get<Ref<Buffer>>(resource.info);
      resource.cost = static_cast<uint32_t>(buffer->size);
    }
  }

  BuildGraph(graph);

  BuildVirtualRoot(graph);

  // Schedule nodes based on heuristic
  ScheduleNodes(graph);

  CalculateResourceLifetimes(graph);

  CHECK_RES(ValidateCompiledGraph(graph));

  CompileResourceTimeline(graph);
  CHECK_ERR(BuildVirtualMemory(context, graph));
  CHECK_ERR(AllocateBlockMemory(context, graph));

  PrintDebug("Allocated {} memory blocks for render graph.",
             graph.memoryBlocks.size());

  CHECK_ERR(AllocateGraphResourceMemory(context, graph));

  PrintDebug("Allocated memory for render graph resources.");

  CHECK_ERR(ValidateResources(graph));

  CHECK_ERR(CreateGraphDescriptorPool(context, graph));

  PrintDebug("Created descriptor pool for render graph.");

  CreateGraphLayoutStates(context, graph);

  CreateGraphResourceTransitions(context, graph);

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

  thread_local std::vector<VkRenderingAttachmentInfo> colorAttachments;
  colorAttachments.clear();
  colorAttachments.resize(maxColorAttachmentIndex + 1);

  bool hasDepthAttachment = false;
  bool hasStencilAttachment = false;

  for (const auto &binding : compiledPass.pass.resourceBindings) {
    if (binding.type == BindingType::Attachment) {
      const auto &resource = graph.resources[binding.resource];
      AttachmentInfo attachInfo =
          GetPassAttachmentInfo(compiledPass.pass, binding);

      auto texture = std::get<Ref<Texture>>(resource.info);

      if (Image::IsDepthTexture(texture->format)) {
        hasDepthAttachment = true;
        continue;
      }

      if (Image::IsStencilTexture(texture->format)) {
        hasStencilAttachment = true;
        continue;
      }

      VkRenderingAttachmentInfo colorAttach = {};
      colorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      colorAttach.loadOp = attachInfo.loadOp;
      colorAttach.storeOp = attachInfo.storeOp;
      colorAttach.clearValue = attachInfo.clearValue;

      colorAttach.imageView = texture->view;
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

  vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

auto Execute(GraphicsContext &context, RenderGraph &graph,
             VkCommandBuffer commandBuffer) -> Error {
  // For each compiled pass, record commands
  for (size_t passIndex = 1; passIndex < graph.compiledPasses.size();
       passIndex++) {
    auto &compiledPass = graph.compiledPasses[passIndex];

    auto transitionResult =
        TransitionImageLayouts(context, graph, compiledPass);
    if (Error::IsError(transitionResult)) {
      return transitionResult;
    }

    if (compiledPass.pass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      BeginPassRendering(context, graph, commandBuffer, compiledPass);
    } else {
      assert(compiledPass.pass.state.bindPoint ==
             VK_PIPELINE_BIND_POINT_COMPUTE);
    }

    auto executeResult = compiledPass.pass.executeFunction(
        commandBuffer, context, graph, compiledPass);
    if (Error::IsError(executeResult)) {
      return executeResult;
    }
  }

  for (const auto &barrier : graph.postGraphUpdates) {
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // src stage
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dst stage
                         0,                                  // dependency flags
                         0, nullptr,                         // memory barriers
                         0, nullptr,                         // buffer barriers
                         1, &barrier                         // image barriers
    );
  }

  return Error::Success();
}

auto AddTexture(RenderGraph &graph, const TextureDescriptor &descriptor)
    -> ResourceHandle {
  assert(descriptor.width > 0 && descriptor.height > 0 &&
         "Texture width and height must be greater than zero");
  assert(descriptor.mipLevels > 0 &&
         "Texture mipLevels must be greater than zero");
  assert(descriptor.depthOrLayers > 0 &&
         "Texture depthOrLayers must be greater than zero");

  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = descriptor.lifetime;
  resource.type = Type::Texture;

  auto texture = Ref<Texture>::Make();

  bool volumeTexture = descriptor.type == TextureType::VOLUME;
  uint32_t depth = volumeTexture ? descriptor.depthOrLayers : 1U;
  uint32_t layers = volumeTexture ? 1U : descriptor.depthOrLayers;

  texture->format = descriptor.format;
  texture->size = {
      .width = descriptor.width, .height = descriptor.height, .depth = depth};
  texture->mipmapcount = descriptor.mipLevels;
  texture->arrayLayers = layers;
  texture->usage = descriptor.usage;
  texture->textureType = descriptor.type;
  texture->samplerDirty = true;

  resource.info = texture;
  graph.resources.emplace_back(resource);

  PrintDebug("Added texture resource with handle {}", resource.handle);
  PrintDebug("  Usage flags: {}", descriptor.usage);

  return resource.handle;
}

auto AddBuffer(RenderGraph &graph, BufferDescriptor descriptor)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = descriptor.lifetime;
  resource.type = Type::Buffer;

  auto buffer = Ref<Buffer>::Make();

  buffer->size = descriptor.size;
  buffer->usage = descriptor.usage;
  buffer->properties = descriptor.memory;

  resource.info = buffer;
  graph.resources.emplace_back(resource);

  return resource.handle;
}

auto ImportTexture(RenderGraph &graph, const Ref<Graphics::Texture> &texture,
                   const LayoutUpdate layoutUpdate) -> ResourceHandle {

  assert(texture->image != VK_NULL_HANDLE &&
         "Imported texture must have a valid image handle");
  assert(texture->mipmapcount > 0 &&
         "Imported texture must have at least one mipmap level");
  assert(texture->arrayLayers > 0 &&
         "Imported texture must have at least one array layer");
  assert(texture->size.width > 0 && "Imported texture must have a valid width");
  assert(texture->size.height > 0 &&
         "Imported texture must have a valid height");
  assert(texture->size.depth > 0 && "Imported texture must have a valid depth");

  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = ResourceLifetime::Persistent;
  resource.type = Type::Texture;
  resource.info = texture;

  assert(texture->view != VK_NULL_HANDLE &&
         "Imported texture must have a valid image view");

  graph.resources.emplace_back(resource);

  graph.initialResourceLayouts[resource.handle] = layoutUpdate.oldState;
  graph.finalResourceLayouts[resource.handle] = layoutUpdate.newState;

  PrintDebug("Imported texture resource with handle {}", resource.handle);
  PrintDebug("  Usage flags: {}", texture->usage);

  return resource.handle;
}

auto ImportBuffer(RenderGraph &graph, const Ref<Graphics::Buffer> &buffer)
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