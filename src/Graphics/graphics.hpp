#pragma once

#include "Graphics/threadContext.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "SDL3/SDL_video.h"

#include "Modules/error.hpp"
#include <cstdint>
#include <mutex>
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
constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 4;

struct SurfaceInfo {
  VkSurfaceFormatKHR format;
  VkPresentModeKHR presentMode;
  VkSurfaceCapabilitiesKHR capabilities;
};

namespace Texture {
struct Texture;
}

struct SwapchainInfo {
  VkSwapchainKHR swapchain;
  VkFormat format;
  VkExtent2D extent;
  uint32_t imageCount;
  std::vector<Ref<Texture::Texture>> textures;
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

auto Initialize(GraphicsContext &context, Window::WindowContext &wcontext)
    -> Error;
auto GetThreadContext() -> ThreadContext &;
auto GetCommandBuffer() -> VkCommandBuffer;
void Deinitialize(GraphicsContext &context);

auto BeginSingleTimeCommands(GraphicsContext &context) -> VkCommandBuffer;

auto EndSingleTimeCommands(GraphicsContext &context,
                           VkCommandBuffer commandBuffer) -> void;

// Graphics context NOLINTNEXTLINE
extern GraphicsContext *g_ctx;
void SetCurrentGraphicsContext(GraphicsContext *ctx);
auto GetCurrentGraphicsContext() -> GraphicsContext *;

auto GetDeferredDestructionAllowed() -> bool &;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::mutex globalTimelineSemaphoreMutex;
thread_local extern std::string ContextDebugname;

extern std::vector<VkCommandPool> CommandPools;
extern std::mutex CommandPoolsMutex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace Graphics