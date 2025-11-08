#pragma once

// #include "SDL3/SDL_video.h"
#include "SDL3/SDL_video.h"

#include "Modules/error.hpp"
#include <cstdint>

#include "volk/volk.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VMA_IMPORT_FUNCTIONS_FROM_VOLK 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

namespace Graphics {
constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct RenderData {
  VkCommandPool pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers;
  std::vector<bool> frameReady;
};

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

struct RuntimeInfo {
  uint32_t textureCount;
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

  std::vector<VkSemaphore> swapchainImageReady;
  std::vector<VkSemaphore> renderingFinished;
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;

  uint64_t currentFrame;
  uint32_t frameIndex;
  uint32_t swapchainImageIndex;

  int32_t renderThreadCount;

  std::vector<RenderData> renderData;

  RuntimeInfo runtimeInfo;
};

struct BlendMode {
  bool enabled;
  VkBlendFactor srcColorBlendFactor;
  VkBlendFactor dstColorBlendFactor;
  VkBlendOp colorBlendOp;
  VkBlendFactor srcAlphaBlendFactor;
  VkBlendFactor dstAlphaBlendFactor;
  VkBlendOp alphaBlendOp;
};

struct GraphicsState {
  VkViewport viewport;
  VkRect2D scissor;
  BlendMode blendMode;
  VkCompareOp depthCompareOp;

  bool depthWriteEnable;
  bool depthTestEnable;

  VkCullModeFlags cullMode;
  VkFrontFace frontFace;

  VkCompareOp stencilCompareOp;
  uint32_t stencilReference;

  bool backfaceCulling;
  bool frontfaceClockwise;

  struct ShaderModule *vertexShader;
  struct ShaderModule *fragmentShader;
  struct ShaderModule *computeShader;
};

auto Initialize(GraphicsContext &context, VkExtent2D dimensions)
    -> Error::Error;
auto GetRenderData(GraphicsContext &context, uint32_t threadIndex)
    -> RenderData;
auto GetCommandBuffer(GraphicsContext &context, uint32_t threadIndex)
    -> VkCommandBuffer;

inline void SetCurrentThreadIndex(int8_t index) {
  thread_local static int8_t current = -1;
  current = index;
}

inline auto GetCurrentThreadIndex() -> int8_t {
  thread_local static int8_t current = -1; // Default to invalid
  return current;
}

// static GraphicsState CurrentGraphicsState = {};

inline auto GetGraphicsState() -> GraphicsState & {
  static thread_local GraphicsState state = {};
  return state;
}

} // namespace Graphics