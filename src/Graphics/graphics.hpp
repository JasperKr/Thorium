#pragma once

#include "Modules/config.hpp"
#include "SDL3/SDL_video.h"

#include "Modules/error.hpp"
#include <cstdint>
#include <vector>

#include "volk/volk.h"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

#define VMA_IMPORT_FUNCTIONS_FROM_VOLK 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

namespace Graphics {
using QueueID = uint32_t;

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
constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 3;

struct SurfaceInfo {
  VkSurfaceFormatKHR format;
  VkPresentModeKHR presentMode;
  VkSurfaceCapabilitiesKHR capabilities;
};

struct SwapchainInfo {
  VkSwapchainKHR swapchain;
  VkFormat format;
  VkExtent2D extent;
  uint32_t imageCount;
  std::vector<VkImage> images;
  std::vector<VkImageView> imageViews;
};

struct GraphicsContext {
  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  VkQueue graphicsQueue;
  uint32_t graphicsQueueFamily;

  SDL_Window *sdlWindow;
  VmaAllocator vmaAllocator;

  SwapchainInfo swapchainInfo;
  SurfaceInfo surfaceInfo;
  VkPhysicalDeviceProperties deviceProperties;
  std::vector<VkDescriptorPool> descriptorPools;

  std::vector<VkSemaphore> swapchainImageReady;
  std::vector<VkSemaphore> renderingFinished;
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;

  uint64_t currentFrame;
  uint32_t frameIndex;
  uint32_t swapchainImageIndex;

  int32_t renderThreadCount;

  VkCommandPool commandPool;
  std::vector<VkCommandBuffer> commandBuffers;
};

auto Initialize(GraphicsContext &context, Config::ApplicationConfig &config)
    -> Error;
auto GetCommandBuffer(GraphicsContext &context) -> VkCommandBuffer;
void Deinitialize(GraphicsContext &context);

inline auto GetCurrentThreadIndex() -> int8_t & {
  thread_local static int8_t current = -1; // Default to invalid
  return current;
}

auto BeginSingleTimeCommands(GraphicsContext &context) -> VkCommandBuffer;

auto EndSingleTimeCommands(GraphicsContext &context,
                           VkCommandBuffer commandBuffer) -> void;

// Graphics context  NOLINTNEXTLINE
static thread_local GraphicsContext *g_ctx = nullptr;
void SetCurrentGraphicsContext(GraphicsContext *ctx);
auto GetCurrentGraphicsContext() -> GraphicsContext *;

auto GetCurrentTimelineSemaphoreValue(GraphicsContext &context)
    -> Result<uint64_t>;
auto InitializeGlobalTimelineSemaphore(GraphicsContext &context) -> Error;
auto DeInitializeGlobalTimelineSemaphore(GraphicsContext &context) -> void;
auto GetCPUTimelineSemaphoreValue(GraphicsContext &context) -> uint64_t &;
auto GetGlobalTimelineSemaphore(GraphicsContext &context) -> VkSemaphore;

} // namespace Graphics