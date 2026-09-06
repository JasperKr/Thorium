#pragma once

#include <string>
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto PipelineStage2ToString(VkPipelineStageFlags2 pipelines) -> std::string;

} // namespace Graphics