#pragma once

#include "Graphics/graphics.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <vector>
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

// Per-Frame global timeline index
// NOLINTNEXTLINE
extern uint64_t GlobalTimelineIndex;

// NOLINTNEXTLINE
extern std::vector<ResourceSync> GlobalResourceSyncTimeline;

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
struct GraphicsResource {

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
};

// NOLINTNEXTLINE global bullshit
extern std::vector<GraphicsResource> Resources;

auto UpdateUsage(GraphicsContext &context, GraphicsResource &resource,
                 const ResourceState &usage) -> void;

inline auto ResetFrameTimeline() -> void {
  GlobalTimelineIndex = 0;
  GlobalResourceSyncTimeline.clear();
}

} // namespace Graphics::Barrier