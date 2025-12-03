#pragma once

#include "color.hpp"
#include "vector.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <vector>
namespace Image {

struct ImageData {
  uint32_t width;
  uint32_t height;
  uint32_t channels;
  VkFormat format;

  std::vector<uint8_t> data;

  auto SetColor(Uvec2 position, const Color &color) -> void;
  auto GetColor(Uvec2 position) -> Color &;

  auto Copy(const ImageData &source) -> void;
};

} // namespace Image