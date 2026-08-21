#pragma once

#include "Modules/error.hpp"
#include <vulkan/vulkan_core.h>

namespace Graphics {
auto BindDescriptorSets(const struct GraphicsContext &context,
                        VkCommandBuffer vkCommandBuffer,
                        VkPipelineStageFlags2 stage) -> Error;
}