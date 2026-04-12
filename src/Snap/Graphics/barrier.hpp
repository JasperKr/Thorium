#pragma once

#include "Graphics/graphicsContext.hpp"
#include <optional>

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <utility>
#include <vector>

struct Texture;

namespace Graphics::Barrier {

struct ResourceState {
  VkPipelineStageFlags2 stages;
  VkAccessFlags2 access;
};

struct ResourceSync {
  VkPipelineStageFlags2 srcStages;
  VkAccessFlags2 srcAccess;
  VkPipelineStageFlags2 dstStages;
  VkAccessFlags2 dstAccess;
};

// As every thread will be recording commands async,
// and these command buffers may be reordered before submission,
// All barriers are thread-local and recorded as needed.

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

// Per-Frame global timeline index
thread_local extern uint64_t GlobalTimelineIndex;

// Timeline index offset for this frame
thread_local extern uint64_t GlobalTimelineOffset;

// The number of barriers issued this frame
thread_local extern uint64_t FrameBarrierCount;

thread_local extern std::vector<ResourceSync> GlobalResourceSyncTimeline;

thread_local extern std::vector<std::pair<struct BarrierSynced, ResourceState>>
    GlobalResourceStateUpdates;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

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
*/

// base class for buffers and textures
struct BarrierSynced {
  BarrierSynced() = default;
  BarrierSynced(const BarrierSynced &) = default;
  BarrierSynced(BarrierSynced &&) = delete;
  auto operator=(const BarrierSynced &) -> BarrierSynced & = default;
  auto operator=(BarrierSynced &&) -> BarrierSynced & = delete;
  virtual ~BarrierSynced() = default;

  // Now, for future me.
  /*
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
  */
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

  // Whether this is the first usage recorded for this resource in the frame
  // When using it for async work.
  // We need to make sure to never add a barrier on first usage since we will insert this later
  // Before the command buffer is stitched together with others. Because the usage is unknown at the time of recording.
  // Due to async recording and reordering.
  bool firstAsyncUsage = false;

  [[nodiscard]] virtual auto IsTexture() const -> bool { return false; }
  [[nodiscard]] virtual auto AsTexture() const -> Texture const * {
    return nullptr;
  }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local std::vector<BarrierSynced> GraphicsResources;

auto UpdateUsage(const GraphicsContext &context, BarrierSynced &resource,
                 const ResourceState &usage) -> void;

auto UpdateUsageVirtual(BarrierSynced &resource, const ResourceState &usage)
    -> std::optional<ResourceSync>;

auto InsertBarrier(ResourceSync &barrier) -> void;
auto ResetFrameTimeline() -> void;
auto ResetModule() -> void;

} // namespace Graphics::Barrier