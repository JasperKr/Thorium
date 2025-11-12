#include "rendergraph.hpp"
#include "Modules/error.hpp"
#include "graphics.hpp"
#include "texture.hpp"
#include "tl/expected.hpp"
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
      pass.readResources.push_back(access.resource);
    } else if (access.accessType == AccessType::Write) {
      pass.writeResources.push_back(access.resource);
    } else if (access.accessType == (AccessType::Read | AccessType::Write)) {
      pass.readwriteResources.push_back(access.resource);
    }
  }

  pass.state.viewport = descriptor.viewport;
  pass.state.scissor = descriptor.scissor;
  pass.state.clearValues = descriptor.clearValues;
  pass.state.bindPoint = descriptor.bindPoint;
  pass.state.blendModes = descriptor.blendModes;
  pass.resourceBindings = descriptor.resourceBindings;

  graph.passes.push_back(pass);

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

    availablePasses.push_back(NodeCost{.handle = passHandle,
                                       .cost = cost,
                                       .childrenCount = static_cast<uint32_t>(
                                           childRenderpass.children.size())});
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
      graph.compiledPasses[parentHandle].children.push_back(
          static_cast<ResourceHandle>(thisIndex));
    }

    // Add to compiled passes
    graph.compiledPasses.push_back(thisPass);
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

      availablePasses.push_back(NodeCost{
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
      graph.virtualRoot.children.push_back(static_cast<ResourceHandle>(i));
    }
  }

  graph.compiledPasses.clear();
  graph.compiledPasses.push_back(graph.virtualRoot);
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
            graph.passes[j].children.push_back(static_cast<ResourceHandle>(i));
            graph.passes[i].parents.push_back(static_cast<ResourceHandle>(j));
          }
        }
      }
    }
  }
}

auto inline ValidateCompiledGraph(const RenderGraph &graph)
    -> tl::expected<bool, Error::Error> {
  // For now just check if the last pass does not write any transient resources
  // It can write persistent resources since they live beyond the graph
  // execution (e.g. swapchain images, or other long-lived targets)

  const auto &lastPass = graph.compiledPasses.back().pass;

  for (const auto &resHandle : lastPass.GetResources(AccessType::Write)) {
    const auto &resource = graph.resources[resHandle];

    if (resource.lifetime == ResourceLifetime::Transient) {
      return tl::make_unexpected(Error::Error{
          .message =
              "Render graph validation failed: last pass writes transient "
              "resource " +
              std::to_string(resHandle),
      });
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

auto inline ReserveBlock(GraphicsContext &context, RenderGraph &graph,
                         uint32_t size = 0) -> Error::Error {
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

  graph.memoryBlocks.push_back(block);

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
auto inline AllocateResourceInBlocks(GraphicsContext &context,
                                     RenderGraph &graph, AllocationInfo info)
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

      graph.virtualAllocations[info.handle] = {.blockIndex = blockIndex,
                                               .resource = info.handle,
                                               .allocation = alloc,
                                               .offset = offset,
                                               .size = info.size};
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
    graph.virtualAllocations[info.handle] = {
        .blockIndex = static_cast<uint32_t>(graph.memoryBlocks.size() - 1),
        .resource = info.handle,
        .allocation = alloc,
        .offset = offset,
        .size = info.size};

    return Error::Success(); // success
  }

  return Error::Create("Failed to allocate virtual memory"); // shits fucked
}

auto inline DeallocateResourceInBlocks(RenderGraph &graph,
                                       const ResourceHandle handle) -> void {
  bool found = false;
  VirtualAllocation *allocation = nullptr;
  for (auto &alloc : graph.virtualAllocations) {
    if (alloc.resource == handle) {
      allocation = &alloc;
      found = true;
      break;
    }
  }

  if (!found) {
    return; // not found
  }

  auto &block = graph.memoryBlocks[allocation->blockIndex];

  vmaVirtualFree(block.virtualBlock, allocation->allocation);

  // Remove from allocations list
  for (auto it = graph.virtualAllocations.begin();
       it != graph.virtualAllocations.end(); ++it) {
    if (it->resource == handle) {
      graph.virtualAllocations.erase(it);
      break;
    }
  }
}

auto inline QueryMemoryAlignmentOfTexture(GraphicsContext &context,
                                          const TextureResource &texRes)
    -> VkDeviceSize {
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = texRes.format;
  imageInfo.extent = texRes.extent;
  imageInfo.mipLevels = texRes.mipLevels;
  imageInfo.arrayLayers = texRes.arrayLayers;
  imageInfo.samples = texRes.samples;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = texRes.usage;
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
                                         const BufferResource &bufRes)
    -> VkDeviceSize {
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = bufRes.size;
  bufferInfo.usage = bufRes.usage;
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

    eventsPerPass[resource.usageLifetime.firstUseIndex].push_back(
        ResourceTimelineEntry{
            .resourceHandle = resource.handle,
            .passHandle = resource.usageLifetime.firstUseIndex,
            .type = ResourceTimelineEntryType::Allocate,
        });
    eventsPerPass[resource.usageLifetime.lastUseIndex].push_back(
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
      graph.compiledResources.push_back(resource);
      if (resource.type == ResourceTimelineEntryType::Allocate) {
        graph.compiledPasses[passIndex].allocations.push_back(resource);
      } else {
        graph.compiledPasses[passIndex].deallocations.push_back(resource);
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

auto inline ConfigurePassAttachments(const TextureInfo &info, RenderPass &pass,
                                     const ResourceBinding &binding) -> void {
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

  VkAttachmentDescription attachmentDesc = {};
  if (info.imported) {
    attachmentDesc.format = info.external.texture.format;
    attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
  } else {
    // We do not support multisampled attachments for now
    // Make sure the sample count is 1 to not confuse the user when it doesn't
    // work
    assert(info.transient.samples == VK_SAMPLE_COUNT_1_BIT);

    attachmentDesc.format = info.transient.format;
    attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
  }
  attachmentDesc.loadOp = loadOp;
  attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDesc.stencilLoadOp = loadOp;
  attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  size_t descriptionIndex = pass.state.attachmentDescriptions.size();
  pass.state.attachmentDescriptions.push_back(attachmentDesc);

  VkAttachmentReference attachmentRef = {};
  attachmentRef.attachment = descriptionIndex;
  attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  pass.state.attachmentReferences[binding.location] = attachmentRef;
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

auto inline ConfigurePassDescriptors(GraphicsContext &context,
                                     RenderGraph &graph, CompiledPass &pass,
                                     const ResourceBinding &binding) -> void {
  // For each pass, configure descriptors for read/write resources
  for (const auto &resHandle : pass.pass.GetResources(
           static_cast<AccessType>(static_cast<uint32_t>(AccessType::Read) |
                                   static_cast<uint32_t>(AccessType::Write)))) {
    const auto &resource = graph.resources[resHandle];

    VkDescriptorImageInfo imageInfo = {};
    VkDescriptorBufferInfo bufferInfo = {};

    if (resource.type == Type::Texture) {
      const auto &texInfo = std::get<TextureInfo>(resource.info);

      // Binding type allows:
      // Sampled: read-only sampler2D
      // Storage: read-write image2D
      auto layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (binding.type == BindingType::Storage) {
        layout = VK_IMAGE_LAYOUT_GENERAL;
      }

      imageInfo.imageLayout = layout;
      // ImageView and Sampler would be set during actual execution
      imageInfo.imageView = VK_NULL_HANDLE;
      imageInfo.sampler = VK_NULL_HANDLE;
    } else if (resource.type == Type::Buffer) {
      const auto &bufInfo = std::get<BufferInfo>(resource.info);

      bufferInfo.offset = 0;
      bufferInfo.range = bufInfo.transient.size;
      // Buffer would be set during actual execution
      bufferInfo.buffer = VK_NULL_HANDLE;
    }

    VkWriteDescriptorSet writeDesc = {};
    writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDesc.dstBinding = binding.binding;
    writeDesc.dstArrayElement = 0;
    writeDesc.descriptorCount = 1;
    writeDesc.descriptorType = GetDescriptorType(resource, binding);
    writeDesc.pImageInfo =
        resource.type == Type::Texture ? &imageInfo : nullptr;
    writeDesc.pBufferInfo =
        resource.type == Type::Buffer ? &bufferInfo : nullptr;

    vkUpdateDescriptorSets(context.device, 1, &writeDesc, 0, nullptr);

    pass.state.descriptorWrites.push_back(writeDesc);
  }
}

auto inline CreateGraphicsPipeline(GraphicsContext &context, RenderGraph &graph,
                                   CompiledPass &compiledPass) -> Error::Error {
  // Create graphics pipeline for the pass
  // For now, we will create a very basic pipeline with no shaders
  // In a real implementation, shaders would be provided per pass

  if (compiledPass.state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Create(
        "Cannot create graphics pipeline for non-graphics pass");
  }

  if (compiledPass.pass.vertexShader == nullptr) {
    return Error::Create(
        "Cannot create graphics pipeline without a vertex shader");
  }

  if (compiledPass.pass.fragmentShader == nullptr) {
    return Error::Create(
        "Cannot create graphics pipeline without a fragment shader");
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = compiledPass.pass.vertexShader->module;
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = compiledPass.pass.fragmentShader->module;
  shaderStages[1].pName = "main";

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.pVertexBindingDescriptions = nullptr;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions = nullptr;
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &compiledPass.state.viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &compiledPass.state.scissor;

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

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask =
      static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT);

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
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
  pipelineInfo.layout = compiledPass.state.pipelineLayout;
  pipelineInfo.renderPass = VK_NULL_HANDLE; // To be set during actual execution
  pipelineInfo.subpass = 0;

  auto error = Error::FromVkResult(vkCreateGraphicsPipelines(
      context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
      &compiledPass.state.pipeline));

  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

auto inline CreateComputePipeline(GraphicsContext &context, RenderGraph &graph,
                                  CompiledPass &compiledPass) -> Error::Error {
  if (compiledPass.state.bindPoint != VK_PIPELINE_BIND_POINT_COMPUTE) {
    return Error::Create("Cannot create compute pipeline for non-compute pass");
  }

  if (compiledPass.pass.computeShader == nullptr) {
    return Error::Create(
        "Cannot create compute pipeline without a compute shader");
  }

  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStage.module = compiledPass.pass.computeShader->module;
  shaderStage.pName = "main";

  VkComputePipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage = shaderStage;
  pipelineInfo.layout = compiledPass.state.pipelineLayout;
  auto error = Error::FromVkResult(
      vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
                               nullptr, &compiledPass.state.pipeline));

  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

auto inline CreatePassPipelines(GraphicsContext &context, RenderGraph &graph)
    -> Error::Error {
  for (auto &compiledPass : graph.compiledPasses) {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    VkPipelineLayout pipelineLayout = nullptr;
    auto error = Error::FromVkResult(vkCreatePipelineLayout(
        context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    if (Error::IsError(error)) {
      return error;
    }

    compiledPass.state.pipelineLayout = pipelineLayout;
    if (compiledPass.state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      auto error = CreateGraphicsPipeline(context, graph, compiledPass);

      if (Error::IsError(error)) {
        return error;
      }

      // vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
      //                           error.value().data(), nullptr,
      //                           &compiledPass.state.pipeline);
    } else if (compiledPass.state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
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

auto inline ConfigureGraphAttachments(GraphicsContext &context,
                                      RenderGraph &graph) -> void {
  // For each pass, configure attachment descriptions and references

  for (auto &compiledPass : graph.compiledPasses) {
    // For each write resource, if it's a texture, add as attachment

    size_t blendmodeCount = compiledPass.state.blendModes.size();
    size_t clearColorCount = compiledPass.state.clearValues.size();

    // Allow default blend mode if none are provided
    // Also allow per-attachment blend modes
    // Or, one blend mode applied to all attachments
    assert(blendmodeCount == 0 ||
           blendmodeCount == compiledPass.pass.writeResources.size() ||
           blendmodeCount == 1);
    // Same logic for clear colors
    assert(clearColorCount == 0 ||
           clearColorCount == compiledPass.pass.writeResources.size() ||
           clearColorCount == 1);

    // Calculate Max location index used in resource bindings
    uint32_t maxLocation = 0;
    for (const auto &binding : compiledPass.resourceBindings) {
      if (binding.type == BindingType::Attachment) {
        maxLocation = (max<uint32_t>)(binding.location, maxLocation);
      }
    }

    compiledPass.state.attachmentReferences.resize(maxLocation + 1);

    for (int i = 0; i < compiledPass.resourceBindings.size(); i++) {
      const auto &binding = compiledPass.resourceBindings[i];
      const auto &resource = graph.resources[binding.resource];

      if (binding.type == BindingType::Attachment) {
        assert(resource.type == Type::Texture &&
               "Only texture resources can be used as attachments");
        const auto &texInfo = std::get<TextureInfo>(resource.info);
        ConfigurePassAttachments(texInfo, compiledPass.pass, binding);
      } else {
        ConfigurePassDescriptors(context, graph, compiledPass, binding);
      }
    }

    compiledPass.state.subpassDescription.colorAttachmentCount =
        static_cast<uint32_t>(compiledPass.state.attachmentDescriptions.size());
    compiledPass.state.subpassDescription.pColorAttachments =
        compiledPass.state.attachmentReferences.data();

    compiledPass.state.subpassDescription.pipelineBindPoint =
        compiledPass.state.bindPoint;
  }
}

auto inline ApplyDescriptorSets(RenderGraph &graph, CompiledPass &pass)
    -> void {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(pass.state.descriptorWrites.size());

  for (auto &descriptorWrite : pass.state.descriptorWrites) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = descriptorWrite.dstBinding;
    binding.descriptorType = descriptorWrite.descriptorType;
    binding.descriptorCount = descriptorWrite.descriptorCount;
    // binding.stageFlags = descriptorWrite.
  }
}

auto inline BuildVirtualMemory(GraphicsContext &context, RenderGraph &graph)
    -> Error::Error {
  // Loop over compiled resource timeline and allocate/deallocate as needed

  graph.virtualAllocations.clear();
  graph.virtualAllocations.resize(graph.resources.size());
  for (const auto &entry : graph.compiledResources) {
    const auto &resource = graph.resources[entry.resourceHandle];

    if (entry.type == ResourceTimelineEntryType::Allocate) {
      VkDeviceSize alignment = 1;
      VkDeviceSize size = 0;

      AllocationInfo allocationInfo = {};
      allocationInfo.handle = resource.handle;

      if (resource.type == Type::Texture) {
        const auto &tex = std::get<TextureInfo>(resource.info).transient;

        // Heuristic size (not actual alloc size)
        auto texels = Texture::GetTexelCount(tex.extent, tex.mipLevels);
        texels *= tex.arrayLayers;
        allocationInfo.size = texels * Texture::GetFormatSize(tex.format);

        allocationInfo.alignment = QueryMemoryAlignmentOfTexture(context, tex);
      } else {
        const auto &buf = std::get<BufferInfo>(resource.info).transient;
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

auto inline AllocateMemory(GraphicsContext &context, RenderGraph &graph)
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

auto Compile(GraphicsContext &context, RenderGraph &graph) -> void {
  // For each resource, calculate cost

  std::cout << "Compiling render graph..." << "\n";

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

  BuildGraph(graph);

  BuildVirtualRoot(graph);

  // Schedule nodes based on heuristic
  ScheduleNodes(graph);

  CalculateResourceLifetimes(graph);

  auto validationResult = ValidateCompiledGraph(graph);
  if (Error::IsError(validationResult)) {
    std::cerr << "Render graph validation error: "
              << validationResult.error().message << "\n";
    return;
  }

  CompileResourceTimeline(graph);
  BuildVirtualMemory(context, graph);
  AllocateMemory(context, graph);
}

auto AddTexture(RenderGraph &graph, const TextureDescriptor &descriptor)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = descriptor.lifetime;
  resource.type = Type::Texture;
  resource.imported = false;

  TextureInfo texInfo = {};

  bool volumeTexture =
      descriptor.type == Texture::TextureType::TEXTURE_TYPE_VOLUME;
  uint32_t depth = volumeTexture ? descriptor.depthOrLayers : 1U;
  uint32_t layers = volumeTexture ? 1U : descriptor.depthOrLayers;

  TextureResource texResource = {};
  texResource.format = descriptor.format;
  texResource.extent = {
      .width = descriptor.width, .height = descriptor.height, .depth = depth};
  texResource.mipLevels = descriptor.mipLevels;
  texResource.arrayLayers = layers;
  texResource.usage = descriptor.usage;
  texResource.samples = VK_SAMPLE_COUNT_1_BIT;

  texInfo.transient = texResource;

  resource.info = texInfo;
  graph.resources.push_back(resource);

  return resource.handle;
}

auto AddBuffer(RenderGraph &graph, BufferDescriptor descriptor)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = descriptor.lifetime;
  resource.type = Type::Buffer;
  resource.imported = false;

  BufferInfo bufInfo = {};

  BufferResource bufResource = {};
  bufResource.size = descriptor.size;
  bufResource.usage = descriptor.usage;

  bufInfo.transient = bufResource;

  resource.info = bufInfo;
  graph.resources.push_back(resource);

  return resource.handle;
}

auto ImportTexture(RenderGraph &graph,
                   const Graphics::Texture::Texture &texture)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = ResourceLifetime::Persistent;
  resource.type = Type::Texture;
  resource.imported = true;

  TextureInfo texInfo = {};

  ImportedTexture importedTex = {};
  importedTex.texture = texture;
  importedTex.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  importedTex.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  texInfo.external = importedTex;
  resource.info = texInfo;
  graph.resources.push_back(resource);

  return resource.handle;
}

auto ImportBuffer(RenderGraph &graph, const Graphics::Buffer &buffer)
    -> ResourceHandle {
  Resource resource = {};
  resource.handle = static_cast<ResourceHandle>(graph.resources.size());
  resource.lifetime = ResourceLifetime::Persistent;
  resource.type = Type::Buffer;
  resource.imported = true;

  BufferInfo bufInfo = {};

  ImportedBuffer importedBuf = {};
  importedBuf.buffer = buffer;

  bufInfo.external = importedBuf;
  resource.info = bufInfo;
  graph.resources.push_back(resource);

  return resource.handle;
}

} // namespace Graphics::Rendergraph