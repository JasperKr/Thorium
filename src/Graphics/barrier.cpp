#include "Graphics/barrier.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstdint>

namespace Graphics::Barrier {

// NOLINTNEXTLINE global bullshit
std::vector<GraphicsResource> Resources;

// Per-Frame global timeline index
// NOLINTNEXTLINE
uint64_t GlobalTimelineIndex = 0;

// NOLINTNEXTLINE
std::vector<ResourceSync> GlobalResourceSyncTimeline{};

inline auto IsHazard(const ResourceState &oldState,
                     const ResourceState &newState) -> bool {
  return ((oldState.access | newState.access) &
          (VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
           VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0U;
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
    uint32_t bitIndex = __builtin_ctzll(bit);
    mask &= ~bit; // clear the lowest set bit

    if ((desiredSynchronization.stages & bit) != 0U) {
      maskBits.at(bitIndex) = desiredSynchronization.access;
    }
  }

  // current timeline index meaning the last barrier that affected this resource
  // and global timeline index the actual current barrier index
  for (uint64_t i = GlobalTimelineIndex; i > currentTimelineIndex; i--) {
    auto &sync = GlobalResourceSyncTimeline[i];

    auto mask = sync.dstStages;
    // for (uint32_t bitIndex = 0; bitIndex < 64; bitIndex++) {
    while (mask != 0U) {
      auto bit = mask & -mask;
      uint32_t bitIndex = __builtin_ctzll(bit);
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
    uint32_t bitIndex = __builtin_ctzll(bit);
    mask &= ~bit;
    if (maskBits.at(bitIndex) != 0U) {
      return true; // Still need barrier
    }
  }

  return false; // All bits satisfied, no barrier needed
}

auto UpdateUsage(GraphicsContext &context, GraphicsResource &resource,
                 const ResourceState &usage) -> void {
  auto &previousAccess = resource.lastUsedAccess;
  auto &previousStages = resource.lastUsedStages;

  if (TimelineLookback(resource.lastUsedTimelineIndex, // NOLINT
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

    vkCmdPipelineBarrier2(
        Graphics::GetCommandBuffer(context, GetCurrentThreadIndex()), &depInfo);

    // Update to new usage
    previousAccess = usage.access;
    previousStages = usage.stages;
    resource.lastUsedTimelineIndex = GlobalTimelineIndex;
    GlobalResourceSyncTimeline.push_back({.srcStages = barrier.srcStageMask,
                                          .srcAccess = barrier.srcAccessMask,
                                          .dstStages = barrier.dstStageMask,
                                          .dstAccess = barrier.dstAccessMask});
    GlobalTimelineIndex++;
  } else {
    // Only used to accumulate read access/stages
    // Since anything involving writes would have caused a hazard and barrier above
    // Not doing this might miss some necessary stages/accesses
    previousAccess |= usage.access;
    previousStages |= usage.stages;
    resource.lastUsedTimelineIndex = GlobalTimelineIndex;
  }
}

} // namespace Graphics::Barrier