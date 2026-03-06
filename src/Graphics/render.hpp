#pragma once

#include "Graphics/graphicsState.hpp"
#include "Graphics/swapchainManager.hpp"
#include "Modules/error.hpp"
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

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern SwapchainManager::SwapchainManager swapchainManager;

auto Present(GraphicsContext &context) -> Error;
auto InitializeRendering(GraphicsContext &context,
                         Window::WindowContext &windowContext) -> Error;
auto DeinitilizeRendering(GraphicsContext &context) -> void;
auto UseCommands(uint64_t key) -> void;
auto UseCommands(const std::string &name) -> void;

} // namespace Graphics