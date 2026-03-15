#pragma once

#include <vulkan/vulkan_core.h>
namespace Graphics {

inline auto GetIsCurrentlyRendering() -> bool & {
  thread_local bool CurrentlyRendering = false;
  return CurrentlyRendering;
}

inline auto GetIsStateDirty() -> bool & {
  thread_local bool StateDirty = true;
  return StateDirty;
}

// Use if the state gets invalidated,
// for example, new command buffer.
inline auto SetDirtyState() -> void { GetIsStateDirty() = true; }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline auto DefaultPixelFormat = VK_FORMAT_R8G8B8A8_UNORM;

constexpr VkPipelineColorBlendAttachmentState DefaultBlendMode = {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT),
};

constexpr uint32_t FRAMES_IN_FLIGHT = 2;
constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 4;

} // namespace Graphics