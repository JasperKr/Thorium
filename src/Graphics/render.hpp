#pragma once

#include "Modules/error.hpp"
#include "graphics.hpp"
#include <string>
#include <vector>
namespace Graphics {

struct StitchInfo {
  // Any amount of buffers to stitch together
  std::array<std::vector<VkCommandBuffer>, FRAMES_IN_FLIGHT> commandBuffers{};
  std::vector<bool> usedCommandBuffers;

  std::vector<uint64_t> orderingKeys;
#ifndef NDEBUG
  std::vector<std::string> orderingNames;
#endif
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern StitchInfo GlobalStitchInfo;

auto Present(GraphicsContext &context) -> Error;
auto InitializeGraphics(GraphicsContext &context) -> Error;
auto UseCommands(uint64_t key) -> void;
auto UseCommands(const std::string &name) -> void;

} // namespace Graphics