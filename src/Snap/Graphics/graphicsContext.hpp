#pragma once

#include "Libraries/vma.hpp"
#include "Modules/object.hpp"
#include "SDL3/SDL_video.h"
#include <mutex>
#include <vector>

#include "volk/volk.h"

#include "texture.hpp"
#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

namespace Graphics {

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
  std::vector<Ref<Texture>> textures;
  std::vector<VkImage> images;
  std::vector<VkImageView> imageViews;
};

struct GraphicsMutexes {
  std::mutex device;
  std::mutex vmaAllocator;
};

struct GraphicsContext {
  static GraphicsMutexes mutexes;

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

  // per frame-in-flight
  std::vector<VkSemaphore> imageAvailable;
  std::vector<VkFence> inFlight;

  // per swapchain image
  std::vector<VkSemaphore> imageReady;
  std::vector<VkFence> imageInFlight;

  uint64_t currentFrame;
  uint32_t frameIndex;
  uint32_t swapchainImageIndex;
};

} // namespace Graphics