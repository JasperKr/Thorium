#pragma once

#include <string>
#include <tuple>
#include <vulkan/vulkan_core.h>

namespace Graphics::BlendMode {

auto ToString(VkBlendFactor blendFactor) -> std::string;
auto ToString(VkBlendOp blendOp) -> std::string;
auto IsDefaultBlendmode(VkBlendFactor srcColorBlendFactor,
                        VkBlendFactor dstColorBlendFactor,
                        VkBlendOp colorBlendOp,
                        VkBlendFactor srcAlphaBlendFactor,
                        VkBlendFactor dstAlphaBlendFactor,
                        VkBlendOp alphaBlendOp) -> bool;
auto ToString(VkBlendFactor srcColorBlendFactor,
              VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp,
              VkBlendFactor srcAlphaBlendFactor,
              VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp)
    -> std::tuple<bool, std::string, std::string>;
auto ToString(VkPipelineColorBlendAttachmentState state)
    -> std::tuple<bool, std::string, std::string>;

} // namespace Graphics::BlendMode