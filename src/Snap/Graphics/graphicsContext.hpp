#pragma once

#include "Libraries/vma.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "SDL3/SDL_video.h"
#include <mutex>
#include <vector>

#include "volk/volk.h"

#include "texture.hpp"
#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

namespace Graphics {

static constexpr uint32_t MaxImageCount = 8;

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
  Math::StackVector<Ref<Texture>, MaxImageCount> textures;
  Math::StackVector<VkImage, MaxImageCount> images;
  Math::StackVector<VkImageView, MaxImageCount> imageViews;
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

  // Frame resources & image are ready for reuse
  Math::StackVector<VkSemaphore, MaxImageCount> imageAvailable;

  // Render finished and ready for presentation, signalled by presentation engine
  Math::StackVector<VkSemaphore, MaxImageCount> renderFinished;

  // Fences to ensure that command buffers have finished executing before being reused
  Math::StackVector<VkFence, MaxImageCount> inFlight;

  uint64_t currentFrame;
  uint32_t frameIndex;
  uint32_t swapchainImageIndex;

  bool currentlyReordering = false;
};

} // namespace Graphics