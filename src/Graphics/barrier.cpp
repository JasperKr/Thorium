#include "Graphics/barrier.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/console.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <unordered_map>

namespace Graphics::Barrier {

// NOLINTNEXTLINE global bullshit
std::vector<ResourceState> GlobalResourceUsageTimeline{};

auto InsertUsage(const ResourceState &usage) -> void {
  GlobalResourceUsageTimeline.emplace_back(usage);
}

inline auto IsHazard(const ResourceState &oldState,
                     const ResourceState &newState) -> bool {
  return ((oldState.access | newState.access) &
          (VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
           VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0U;
}

auto FlushBarriers(GraphicsContext &context,
                   const ResourceState &srcState, // NOLINT
                   const ResourceState &dstState) -> void {

  VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                           nullptr,
                           srcState.stages,
                           srcState.access,
                           dstState.stages,
                           dstState.access};

  for (auto &resource : Resources) {
    for (uint32_t bit = 0; bit < sizeof(uint32_t) * 4U; ++bit) {
      VkPipelineStageFlags2 stageFlag = 1U << bit;
      bool bitInBarrier = (srcState.stages & stageFlag) != 0U;
      if (bitInBarrier && resource.LastUsedStates.contains(stageFlag)) {
        VkAccessFlags2 &accessFlag = resource.LastUsedStates.at(stageFlag);
        accessFlag &= ~srcState.access;
      }
    }
  }

  VkDependencyInfo depInfo{};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.memoryBarrierCount = 1;
  depInfo.pMemoryBarriers = &barrier;

  if (GetIsCurrentlyRendering()) {
    vkCmdEndRendering(GetCommandBuffer(context, GetCurrentThreadIndex()));
    GetIsCurrentlyRendering() = false;
  }

  vkCmdPipelineBarrier2(GetCommandBuffer(context, GetCurrentThreadIndex()),
                        &depInfo);
}

} // namespace Graphics::Barrier