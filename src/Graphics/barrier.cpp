#include "Graphics/barrier.hpp"
#include "Graphics/graphicsState.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>
#include <utility>
#include <vector>

namespace Graphics::Barrier {

// Per-Frame global timeline index
// NOLINTNEXTLINE
thread_local uint64_t GlobalTimelineIndex = 0;

// Timeline index offset for this frame
// NOLINTNEXTLINE
thread_local uint64_t GlobalTimelineOffset = 0;

// The number of barriers issued this frame
// NOLINTNEXTLINE
thread_local uint64_t FrameBarrierCount = 0;

// NOLINTNEXTLINE
thread_local std::vector<ResourceSync> GlobalResourceSyncTimeline{};

thread_local std::vector<std::pair<BarrierSynced, ResourceState>>
    GlobalResourceStateUpdates{}; // NOLINT

inline auto IsHazard(const ResourceState &oldState,
                     const ResourceState &newState) -> bool {
  return ((oldState.access | newState.access) &
          (VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
           VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0U;
}

// Count trailing bits set in a 64-bit integer
// Staring from least significant bit to most significant bit
inline auto TrailingBitCount(uint64_t bits) -> uint32_t {
  if (bits == 0U) {
    return 64U; // No bits set NOLINT
  }
#if defined(_MSC_VER)
  return static_cast<uint32_t>(__tzcnt_u64(bits));
#else
  return __builtin_ctzll(bits);
#endif
}

/*

For example.

cs: write buffer 1 // epoch 0
cs: write buffer 2 // epoch 0
barrier: CS -> CS, Write -> Read // epoch 1, add syncs[1] = this barrier
cs: read buffer 1 // epoch 1
// barrier: CS -> CS, Write -> Read (already done)
cs: read buffer 2 // epoch 1
barrier: WaW // epoch 2, add syncs[2] = this barrier, set all affected to 2
cs: write buffer 1 // epoch 2
// no barrier needed since last usage epoch: 2 <= current, 2, so up-to-date
cs: write buffer 2 // epoch 2
barrier: WaW // epoch 3, etc, since last usage epoch: 2 <= current, 3, not up-to-date
cs: write buffer 2 // epoch 3

now any time we do a command we lookback in the global syncs table starting at the current epoch
to see if we need to sync by checking our last usage and looking back in time if we flushed for that by the time we reached now.

// base class for buffers and textures
struct GraphicsResource {

  // Now, for future me.
  We might be inclined to make this a map. For example:
  // Compute shader writes to ssbo idx 0
  // Fragment 1 shader writes to ssbo idx 1
  // Fragment 2 shader reads from ssbo idx 0
  // now, if we look at the aspect flags:
  // FRAG|COMP, READ|WRITE
  // we can't really represent that with a single set of flags.
  // But! think about it, we would already have gotten a WaW hazard
  // so we would have flushed the barriers before getting here.
  // timeline:
  // write to idx 0
  // flush
  // write to idx 1
  // flush
  // read from idx 0
  // whatever happens next.
  VkPipelineStageFlags2 lastUsedStages = 0;
  VkAccessFlags2 lastUsedAccess = 0;

  // To make sure this scenario is handled correctly:
  // CS write to buffer 1
  // CS write to buffer 2
  // Barrier CS -> CS, Write -> Read
  // CS read from buffer 1
  // No barrier here because of previous barrier
  // CS read from buffer 2
  // We need to know when stuff was synced last
  uint64_t lastUsedTimelineIndex = 0;
};

*/

auto bitsIncludeAll(uint64_t bits, uint64_t testBits) -> bool {
  return (bits & testBits) == testBits;
}

// Look back in timeline to see if we already synced for this resource
inline auto TimelineLookback(uint64_t currentTimelineIndex,
                             const ResourceState &resourceLastUsage, // NOLINT
                             const ResourceState &desiredSynchronization)
    -> bool {

  if (!IsHazard(resourceLastUsage, desiredSynchronization)) {
    return false; // No hazard, no barrier needed
  }

  // NOLINTNEXTLINE, magic number 64 for max bits in flags
  std::array<uint64_t, 64> maskBits{};

  // Store unsynced access bits per stage bit
  // so we can early out when all bits are satisfied

  // Fast way to iterate over set bits
  auto mask = desiredSynchronization.stages;
  while (mask != 0U) {       // while there are still bits set
    auto bit = mask & -mask; // negative of a number isolates the lowest set bit

    // count trailing zeros after removing the latest bit to get index
    uint32_t bitIndex = TrailingBitCount(bit);
    mask &= ~bit; // clear the lowest set bit

    // NOLINTNEXTLINE
    if ((desiredSynchronization.stages & bit) != 0U && bitIndex != 64U) {
      maskBits.at(bitIndex) = desiredSynchronization.access;
    }
  }

  uint64_t LocalTimelineIndex = GlobalTimelineIndex - GlobalTimelineOffset;

  if (LocalTimelineIndex == 0 || currentTimelineIndex >= LocalTimelineIndex) {
    return true; // No barriers yet, need to sync
  }

  // current timeline index meaning the last barrier that affected this resource
  // and global timeline index the actual current barrier index
  for (uint64_t i = GlobalTimelineIndex - 1ULL; i > currentTimelineIndex; i--) {
    assert(i - GlobalTimelineOffset < GlobalResourceSyncTimeline.size() &&
           "Timeline index out of bounds in lookback");
    auto &sync = GlobalResourceSyncTimeline[i - GlobalTimelineOffset];

    auto mask = sync.dstStages;
    while (mask != 0U) {
      auto bit = mask & -mask;
      uint32_t bitIndex = TrailingBitCount(bit);

      if (bitIndex == 64U) { // NOLINT
        break;               // No bits set
      }

      mask &= ~bit;
      // If this stage bit is active in this sync and we still need it

      bool srcMatchesForThisStage =
          // Match the sync's source stage with our current bit
          (sync.srcStages & bit) != 0U &&
          // Match the sync's source access with our resource's last usage
          (sync.srcAccess & resourceLastUsage.access) != 0U;
      bool accessMatchesRequiredSync =
          // Match the sync's destination access with our desired sync access
          (maskBits.at(bitIndex) & sync.dstAccess) != 0U;

      if (srcMatchesForThisStage && accessMatchesRequiredSync) {
        // Remove the accesses that are now satisfied
        maskBits.at(bitIndex) &= ~sync.dstAccess;
      }
    }
  }

  mask = desiredSynchronization.stages;
  while (mask != 0U) {
    auto bit = mask & -mask;
    uint32_t bitIndex = TrailingBitCount(bit);
    mask &= ~bit;
    if (maskBits.at(bitIndex) != 0U) {
      return true; // Still need barrier
    }
  }

  return false; // All bits satisfied, no barrier needed
}

auto UpdateUsage(const GraphicsContext &context, BarrierSynced &resource,
                 const ResourceState &usage) -> void {
  ZoneScoped;

  auto &previousAccess = resource.lastUsedAccess;
  auto &previousStages = resource.lastUsedStages;

  // Keep track of first usage for async recording so we can barrier later
  if (resource.firstAsyncUsage) {
    GlobalResourceStateUpdates.emplace_back(resource, usage);
  }

  if (!resource.firstAsyncUsage &&
      TimelineLookback(resource.lastUsedTimelineIndex, // NOLINT
                       {.stages = previousStages, .access = previousAccess},
                       usage)) {
    // Insert barrier
    VkMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = previousStages;
    barrier.srcAccessMask = previousAccess;
    barrier.dstStageMask = usage.stages;
    barrier.dstAccessMask = usage.access;

    VkDependencyInfo depInfo = {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &barrier;

    if (GetIsCurrentlyRendering()) {
      // End rendering before doing a barrier
      vkCmdEndRendering(Graphics::GetCommandBuffer());
      GetIsCurrentlyRendering() = false;
    }

    vkCmdPipelineBarrier2(Graphics::GetCommandBuffer(), &depInfo);

    // Update to new usage
    previousAccess = usage.access;
    previousStages = usage.stages;
    resource.lastUsedTimelineIndex = GlobalTimelineIndex;
    GlobalResourceSyncTimeline.push_back({.srcStages = barrier.srcStageMask,
                                          .srcAccess = barrier.srcAccessMask,
                                          .dstStages = barrier.dstStageMask,
                                          .dstAccess = barrier.dstAccessMask});
    GlobalTimelineIndex++;
    FrameBarrierCount++;
  } else {
    // Only used to accumulate read access/stages
    // Since anything involving writes would have caused a hazard and barrier above
    // Not doing this might miss some necessary stages/accesses
    previousAccess |= usage.access;
    previousStages |= usage.stages;
    resource.lastUsedTimelineIndex = GlobalTimelineIndex;
  }

  resource.firstAsyncUsage = false;
}

// The same as Update Usage but doesn't insert any barriers
auto UpdateUsageVirtual(BarrierSynced &resource, const ResourceState &usage)
    -> std::optional<ResourceSync> {
  auto &previousAccess = resource.lastUsedAccess;
  auto &previousStages = resource.lastUsedStages;

  // Keep track of first usage for async recording so we can barrier later
  if (resource.firstAsyncUsage) {
    GlobalResourceStateUpdates.emplace_back(resource, usage);
  }

  if (!resource.firstAsyncUsage &&
      TimelineLookback(resource.lastUsedTimelineIndex, // NOLINT
                       {.stages = previousStages, .access = previousAccess},
                       usage)) {
    resource.lastUsedTimelineIndex = GlobalTimelineIndex;

    auto sync = ResourceSync{.srcStages = previousStages,
                             .srcAccess = previousAccess,
                             .dstStages = usage.stages,
                             .dstAccess = usage.access};

    // Update to new usage
    previousAccess = usage.access;
    previousStages = usage.stages;

    GlobalResourceSyncTimeline.emplace_back(sync);
    GlobalTimelineIndex++;
    FrameBarrierCount++;

    return sync;
  }

  // Only used to accumulate read access/stages
  // Since anything involving writes would have caused a hazard and barrier above
  // Not doing this might miss some necessary stages/accesses
  previousAccess |= usage.access;
  previousStages |= usage.stages;
  resource.lastUsedTimelineIndex = GlobalTimelineIndex;

  resource.firstAsyncUsage = false;

  return std::nullopt;
}

auto InsertBarrier(ResourceSync &barrier) -> void {
  GlobalResourceSyncTimeline.push_back(barrier);
  FrameBarrierCount++;
}

auto ResetFrameTimeline() -> void {
  GlobalTimelineOffset += FrameBarrierCount;
  GlobalResourceSyncTimeline.clear();
  GlobalResourceStateUpdates.clear();

  FrameBarrierCount = 0;
}

auto ResetModule() -> void {
  GlobalTimelineIndex = 0;
  GlobalTimelineOffset = 0;
  FrameBarrierCount = 0;
  GlobalResourceSyncTimeline.clear();
  GlobalResourceStateUpdates.clear();

  for (auto &res : GraphicsResources) {
    res.firstAsyncUsage = true;
  }
}

} // namespace Graphics::Barrier