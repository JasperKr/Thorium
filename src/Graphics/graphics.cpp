#include "graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/window.hpp"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include "tl/expected.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Graphics {

GraphicsMutexes GraphicsContext::mutexes = {};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

GraphicsContext *g_ctx = nullptr;

thread_local std::string ContextDebugname{};

std::vector<VkCommandPool> CommandPools{};
std::mutex CommandPoolsMutex{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto GetDeferredDestructionAllowed() -> bool & {
  static bool deferredDestructionAllowed = true;
  return deferredDestructionAllowed;
}

inline auto
FindHDRColorspaceSupport(const GraphicsContext &context,
                         const std::vector<VkSurfaceFormatKHR> &formats)
    -> Result<VkSurfaceFormatKHR> {
  constexpr std::array<VkFormat, 2> AllowedFormats = {
      VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32};
  constexpr std::array<VkColorSpaceKHR, 2> AllowedColorSpaces = {
      VK_COLOR_SPACE_HDR10_HLG_EXT, VK_COLOR_SPACE_HDR10_ST2084_EXT};

  constexpr std::array Preferred = {
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_A2B10G10R10_UNORM_PACK32,
      VK_FORMAT_A2R10G10B10_UNORM_PACK32,
  };

  for (VkFormat preferredFormat : Preferred) {
    for (VkColorSpaceKHR colorspace : AllowedColorSpaces) {
      for (const auto &format : formats) {
        if (format.format == preferredFormat &&
            format.colorSpace == colorspace) {
          return format;
        }
      }
    }
  }

  return Error::Unexpected("No suitable HDR surface format found", -1);
}

inline auto
FindSRGBColorspaceSupport(const GraphicsContext &context,
                          const std::vector<VkSurfaceFormatKHR> &formats)
    -> Result<VkSurfaceFormatKHR> {
  constexpr std::array<VkFormat, 2> AllowedFormats = {VK_FORMAT_R8G8B8A8_SRGB,
                                                      VK_FORMAT_B8G8R8A8_SRGB};

  for (const auto &format : formats) {
    if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      continue;
    }

    for (const auto &allowedFormat : AllowedFormats) {
      if (format.format == allowedFormat) {
        return format;
      }
    }
  }

  return Error::Unexpected("No suitable sRGB surface format found", -1);
}

inline auto
FindLinearColorspaceSupport(const GraphicsContext &context,
                            const std::vector<VkSurfaceFormatKHR> &formats)
    -> Result<VkSurfaceFormatKHR> {
  constexpr std::array<VkFormat, 2> AllowedFormats = {VK_FORMAT_R8G8B8A8_UNORM,
                                                      VK_FORMAT_B8G8R8A8_UNORM};

  for (const auto &format : formats) {
    if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      continue;
    }

    for (const auto &allowedFormat : AllowedFormats) {
      if (format.format == allowedFormat) {
        return format;
      }
    }
  }

  return Error::Unexpected("No suitable linear surface format found", -1);
}

static auto FindSurfaceFormat(Window::ColorSpace colorspace,
                              GraphicsContext &context)
    -> Result<VkSurfaceFormatKHR> {
  // Surface format finding code here

  uint32_t formatCount = 0;

  vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface,
                                       &formatCount, nullptr);
  if (formatCount == 0) {
    return Error::Unexpected("No surface formats found", -1);
  }

  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface,
                                       &formatCount, formats.data());

  switch (colorspace) {
  case Window::ColorSpace::GammaCorrect:
    return FindSRGBColorspaceSupport(context, formats);
  case Window::ColorSpace::Linear:
    return FindLinearColorspaceSupport(context, formats);
  case Window::ColorSpace::HDR:
    return FindHDRColorspaceSupport(context, formats);
  default:
    return Error::Unexpected("No suitable surface format found", -2);
  }
}

static auto FindPhysicalDevice(GraphicsContext &context) -> Error {
  uint32_t gpuCount = 0;
  Error error = Error::Create(
      vkEnumeratePhysicalDevices(context.instance, &gpuCount, nullptr));

  if (gpuCount == 0) {
    return Error::Create("No Vulkan-compatible GPUs found.");
  }

  if (Error::IsError(error)) {
    return error;
  }

  std::vector<VkPhysicalDevice> gpus(gpuCount);
  error = Error::Create(
      vkEnumeratePhysicalDevices(context.instance, &gpuCount, gpus.data()));

  if (Error::IsError(error)) {
    return error;
  }

  // Keep a score of the best GPU found
  int bestGpuIndex = -1;
  int bestGpuScore = -1;

  const int DiscreteGPUScore = 1000;

  for (uint32_t i = 0; i < gpuCount; i++) {
    VkPhysicalDeviceProperties2 deviceProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };

    vkGetPhysicalDeviceProperties2(gpus.at(i), &deviceProperties);

    int score = 0;

    // Prefer discrete GPUs
    if (deviceProperties.properties.deviceType ==
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += DiscreteGPUScore;
    }

    // Higher max image dimension gets a higher score
    score += static_cast<int>(
        deviceProperties.properties.limits.maxImageDimension2D);

    if (score > bestGpuScore) {
      bestGpuScore = score;
      bestGpuIndex = static_cast<int>(i);
      context.deviceProperties = deviceProperties.properties;
    }
  }

  if (bestGpuIndex == -1) {
    return Error::Create("Failed to find a suitable GPU.");
  }

  context.physicalDevice = gpus.at(bestGpuIndex);

  return Error::Success();
}

static auto FindQueueFamilies(GraphicsContext &context) -> Error {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice,
                                           &queueFamilyCount, nullptr);

  if (queueFamilyCount == 0) {
    return Error::Create("No queue families found on physical device.");
  }

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(
      context.physicalDevice, &queueFamilyCount, queueFamilies.data());

  bool found = false;
  int selectedIndex = -1;

  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i,
                                         context.surface, &presentSupport);

    if ((queueFamilies.at(i).queueFlags &
         static_cast<uint32_t>(VK_QUEUE_GRAPHICS_BIT)) != 0 &&
        presentSupport == VK_TRUE) {
      found = true;
      selectedIndex = static_cast<int>(i);
      break;
    }
  }

  if (!found) {
    return Error::Create("No suitable queue family found.");
  }

  context.graphicsQueueFamily = static_cast<uint32_t>(selectedIndex);
  return Error::Success();
}

using PM = VkPresentModeKHR;

// If this is ever edited, make sure the present modes are in the same order as
// The VkPresentModeKHR enum, Immediate = 0, Mailbox = 1, FIFO = 2, FIFO_Relaxed = 3
constexpr std::array<std::array<PM, 6>, 4> PreferencePerPresentMode = {{
    {
        PM::VK_PRESENT_MODE_IMMEDIATE_KHR,
        PM::VK_PRESENT_MODE_MAILBOX_KHR,
        PM::VK_PRESENT_MODE_FIFO_KHR,
        PM::VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        PM::VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,
        PM::VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,
    },
    {
        PM::VK_PRESENT_MODE_MAILBOX_KHR,
        PM::VK_PRESENT_MODE_IMMEDIATE_KHR,
        PM::VK_PRESENT_MODE_FIFO_KHR,
        PM::VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        PM::VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,
        PM::VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,
    },
    {
        PM::VK_PRESENT_MODE_FIFO_KHR,
        PM::VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        PM::VK_PRESENT_MODE_MAILBOX_KHR,
        PM::VK_PRESENT_MODE_IMMEDIATE_KHR,
        PM::VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,
        PM::VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,
    },
    {
        PM::VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        PM::VK_PRESENT_MODE_FIFO_KHR,
        PM::VK_PRESENT_MODE_MAILBOX_KHR,
        PM::VK_PRESENT_MODE_IMMEDIATE_KHR,
        PM::VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,
        PM::VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,
    },
}};

static auto FindPresentMode(Window::WindowContext &wcontext,
                            GraphicsContext &context)
    -> Result<VkPresentModeKHR> {
  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      context.physicalDevice, context.surface, &presentModeCount, nullptr);

  if (presentModeCount == 0) {
    return Error::Unexpected("No present modes found for surface.", -1);
  }

  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(context.physicalDevice,
                                            context.surface, &presentModeCount,
                                            presentModes.data());

  auto Preferred = VK_PRESENT_MODE_MAX_ENUM_KHR;

  switch (wcontext.vsync) {
  case Window::VsyncMode::Immediate:
    Preferred = VK_PRESENT_MODE_IMMEDIATE_KHR;
    break;
  case Window::VsyncMode::Replace:
    Preferred = VK_PRESENT_MODE_MAILBOX_KHR;
    break;
  case Window::VsyncMode::Enabled:
    Preferred = VK_PRESENT_MODE_FIFO_KHR;
    break;
  case Window::VsyncMode::Adaptive:
    Preferred = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    break;
  }

  const auto &preferenceList = PreferencePerPresentMode.at(Preferred);
  for (const auto &preferredMode : preferenceList) {
    for (const auto &availableMode : presentModes) {
      if (preferredMode == availableMode) {
        return availableMode;
      }
    }
  }

  return Error::Unexpected("No suitable present mode found for surface.", -2);
}

static auto CreateDevice(GraphicsContext &context) -> Error {
  float queuePriority = 1.0F;
  VkDeviceQueueCreateInfo queueCreateInfo{};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = context.graphicsQueueFamily;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = &queueCreateInfo;
  createInfo.queueCreateInfoCount = 1;
  createInfo.pEnabledFeatures = &deviceFeatures;

  // --- Vulkan 1.3 features ---
  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE,
  };

  PrintDebug("Enabled Vulkan 1.3 features: synchronization2, dynamicRendering");

  // --- Vulkan 1.2 features ---
  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &features13,
      .descriptorIndexing = VK_TRUE,
      .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
      .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
      .runtimeDescriptorArray = VK_TRUE,
      .timelineSemaphore = VK_TRUE,
      .bufferDeviceAddress = VK_TRUE,
  };

  PrintDebug(
      "Enabled Vulkan 1.2 features: descriptorIndexing, "
      "shaderSampledImageArrayNonUniformIndexing, "
      "shaderStorageBufferArrayNonUniformIndexing, runtimeDescriptorArray, "
      "timelineSemaphore, bufferDeviceAddress");

  // --- Vulkan 1.1 features ---
  VkPhysicalDeviceVulkan11Features features11{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &features12,
  };

  // --- Dynamic Rendering ---
  // VkPhysicalDeviceDynamicRenderingFeatures dynRender{
  //     .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
  //     .pNext = &features11,
  //     .dynamicRendering = VK_TRUE,
  // };

  // --- Features2 root ---
  VkPhysicalDeviceFeatures2 features2{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &features11,
  };

  // Device create info
  createInfo.pNext = &features2;
  createInfo.pEnabledFeatures = nullptr;

  const std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  Error error = Error::Create(vkCreateDevice(
      context.physicalDevice, &createInfo, nullptr, &context.device));

  if (Error::IsError(error)) {
    return error;
  }

  PrintDebug("Loading Vulkan device with Volk...");
  volkLoadDevice(context.device);

  vkGetDeviceQueue(context.device, context.graphicsQueueFamily, 0,
                   &context.graphicsQueue);

  return Error::Success();
}

auto GetThreadContext() -> ThreadContext & {
  static thread_local ThreadContext threadContext;
  return threadContext;
}

auto GetCommandBuffer() -> VkCommandBuffer {
  auto *commandBuffer = GetThreadContext().commandBuffer;
  assert(commandBuffer != nullptr && "Command buffer is null!");
  return commandBuffer;
}

static auto CreateSemaphores(GraphicsContext &context) -> Error {
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  context.swapchainImageReady = std::vector<VkSemaphore>(MAX_SWAPCHAIN_IMAGES);
  context.renderingFinished = std::vector<VkSemaphore>(MAX_SWAPCHAIN_IMAGES);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    for (int i = 0; i < MAX_SWAPCHAIN_IMAGES; i++) {
      Error error = Error::Create(
          vkCreateSemaphore(context.device, &semaphoreInfo, nullptr,
                            &context.swapchainImageReady.at(i)));

      if (Error::IsError(error)) {
        return error;
      }
    }

    for (int i = 0; i < MAX_SWAPCHAIN_IMAGES; i++) {
      Error error = Error::Create(
          vkCreateSemaphore(context.device, &semaphoreInfo, nullptr,
                            &context.renderingFinished.at(i)));

      if (Error::IsError(error)) {
        return error;
      }
    }
  }

  return Error::Success();
}

static auto CreateFences(GraphicsContext &context) -> Error {
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  context.inFlightFences = std::vector<VkFence>(FRAMES_IN_FLIGHT);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
      Error error = Error::Create(vkCreateFence(
          context.device, &fenceInfo, nullptr, &context.inFlightFences.at(i)));
    }
  }

  if (context.swapchainInfo.imageCount <= 0) {
    return Error::Create("Swapchain image count is zero.");
  }

  context.imagesInFlight =
      std::vector<VkFence>(context.swapchainInfo.imageCount);

  for (int i = 0; i < (int32_t)context.swapchainInfo.imageCount; i++) {
    context.imagesInFlight.at(i) = VK_NULL_HANDLE;
  }

  return Error::Success();
}

static auto CreateVmaAllocator(GraphicsContext &context) -> Error {
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  std::lock_guard<std::mutex> lock2(
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  VmaAllocatorCreateInfo allocatorInfo = {0};
  allocatorInfo.physicalDevice = context.physicalDevice;
  allocatorInfo.device = context.device;
  allocatorInfo.instance = context.instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
  allocatorInfo.pAllocationCallbacks = nullptr;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;

  VmaVulkanFunctions vulkanFunctions;
  Error error = Error::Create(
      vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions));

  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  error =
      Error::Create(vmaCreateAllocator(&allocatorInfo, &context.vmaAllocator));

  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

inline auto CreateCommandPool(ThreadContext &tcontext) -> Error {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = tcontext.graphicsContext->graphicsQueueFamily;
  poolInfo.flags =
      static_cast<uint32_t>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) |
      static_cast<uint32_t>(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    Error error = Error::Create(
        vkCreateCommandPool(tcontext.graphicsContext->device, &poolInfo,
                            nullptr, &tcontext.commandPool));

    if (Error::IsError(error)) {
      return error;
    }
  }

  return Error::Success();
}

auto GetSwapchainTextures(GraphicsContext &context) -> Error {
  context.swapchainInfo.textures.clear();
  context.swapchainInfo.textures.resize(context.swapchainInfo.imageCount);

  for (uint32_t i = 0; i < context.swapchainInfo.imageCount; i++) {
    auto textureResult = Graphics::Texture::FromSwapchainTexture(
        context, context.swapchainInfo.images[i],
        context.swapchainInfo.imageViews[i], context.swapchainInfo.format,
        context.swapchainInfo.extent.width,
        context.swapchainInfo.extent.height);

    if (Error::IsError(textureResult)) {
      return textureResult.error();
    }

    context.swapchainInfo.textures.at(i) = textureResult.value();
  }

  return Error::Success();
}

inline auto CreateVkSwapchain(GraphicsContext &context) -> Error {
  VkSwapchainCreateInfoKHR swapchainInfo = {};
  swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainInfo.surface = context.surface;

  VkSurfaceCapabilitiesKHR surfaceCapabilities =
      context.surfaceInfo.capabilities;
  VkSurfaceFormatKHR surfaceFormat = context.surfaceInfo.format;
  VkPresentModeKHR presentMode = context.surfaceInfo.presentMode;

  swapchainInfo.minImageCount = surfaceCapabilities.minImageCount;

  if (surfaceCapabilities.maxImageCount > 0 &&
      swapchainInfo.minImageCount > surfaceCapabilities.maxImageCount) {
    swapchainInfo.minImageCount = surfaceCapabilities.maxImageCount;
  }

  int width = 0;
  int height = 0;
  SDL_GetWindowSizeInPixels(context.sdlWindow, &width, &height);

  PrintAlways("Creating swapchain with resolution {}x{}.", width, height);

  VkExtent2D extent = {(uint32_t)width, (uint32_t)height};

  swapchainInfo.imageFormat = surfaceFormat.format;
  swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapchainInfo.imageExtent = extent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.preTransform = surfaceCapabilities.currentTransform;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = presentMode;
  swapchainInfo.clipped = VK_TRUE;
  swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    Error error = Error::Create(vkCreateSwapchainKHR(
        context.device, &swapchainInfo, nullptr, &swapchain));

    if (Error::IsError(error)) {
      return error;
    }
  }

  context.swapchainInfo.swapchain = swapchain;
  context.swapchainInfo.format = surfaceFormat.format;
  context.swapchainInfo.extent = swapchainInfo.imageExtent;

  vkGetSwapchainImagesKHR(context.device, swapchain,
                          &context.swapchainInfo.imageCount, nullptr);

  context.swapchainInfo.images.resize(context.swapchainInfo.imageCount);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    Error error = Error::Create(vkGetSwapchainImagesKHR(
        context.device, swapchain, &context.swapchainInfo.imageCount,
        context.swapchainInfo.images.data()));

    if (Error::IsError(error)) {
      return error;
    }
  }

  context.swapchainInfo.imageViews.resize(context.swapchainInfo.imageCount);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    for (uint32_t i = 0; i < context.swapchainInfo.imageCount; i++) {
      VkImageViewCreateInfo imageViewInfo = {};

      imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      imageViewInfo.image = context.swapchainInfo.images[i];
      imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewInfo.format = context.swapchainInfo.format;
      imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageViewInfo.subresourceRange.baseMipLevel = 0;
      imageViewInfo.subresourceRange.levelCount = 1;
      imageViewInfo.subresourceRange.baseArrayLayer = 0;
      imageViewInfo.subresourceRange.layerCount = 1;

      VkImageView imageView = VK_NULL_HANDLE;
      Error error = Error::Create(vkCreateImageView(
          context.device, &imageViewInfo, nullptr, &imageView));

      context.swapchainInfo.imageViews[i] = imageView;
    }
  }

  auto result = GetSwapchainTextures(context);
  if (Error::IsError(result)) {
    return result;
  }

  PrintInfo("Swapchain has " +
            std::to_string(context.swapchainInfo.imageCount) + " images.");

  return Error::Success();
}

auto CreateSwapchain(GraphicsContext &context, Window::WindowContext &wcontext)
    -> Error {
  PrintWarning("Creating swapchain...");

  auto presentResult = FindPresentMode(wcontext, context);
  if (Error::IsError(presentResult)) {
    return presentResult.error();
  }
  VkPresentModeKHR presentMode = presentResult.value();

  context.surfaceInfo.presentMode = presentMode;

  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      context.physicalDevice, context.surface, &surfaceCapabilities);

  auto surfaceResult = FindSurfaceFormat(wcontext.colorSpace, context);
  if (Error::IsError(surfaceResult)) {
    return surfaceResult.error();
  }
  VkSurfaceFormatKHR surfaceFormat = surfaceResult.value();

  context.surfaceInfo.format = surfaceFormat;
  context.surfaceInfo.capabilities = surfaceCapabilities;

  for (auto *const fence : context.imagesInFlight) {
    if (fence == VK_NULL_HANDLE) {
      continue;
    }

    vkWaitForFences(context.device, 1, &fence, VK_TRUE, UINT64_MAX);
  }

  for (auto *const view : context.swapchainInfo.imageViews) {
    vkDestroyImageView(context.device, view, nullptr);
  }
  context.swapchainInfo.imageViews.clear();

  if (context.swapchainInfo.swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(context.device, context.swapchainInfo.swapchain,
                          nullptr);
    context.swapchainInfo.swapchain = VK_NULL_HANDLE;
  }

  auto result = CreateVkSwapchain(context);
  if (Error::IsError(result)) {
    return result;
  }

  wcontext.swapchainOutOfDate = false;

  return Error::Success();
}

auto Initialize(GraphicsContext &context, Window::WindowContext &wcontext)
    -> Error {
  PrintDebug("Initializing Volk...");
  Error error = Error::Create(volkInitialize());

  // Initialize SDL for Vulkan
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    return Error::Create(SDL_GetError());
  }

  SDL_Window *window = SDL_CreateWindow(
      wcontext.initialSettings.title.c_str(), wcontext.initialSettings.width,
      wcontext.initialSettings.height,
      SDL_WINDOW_VULKAN | wcontext.initialSettings.GetSDLWindowFlags());

  if (window == nullptr) {
    return Error::Create("Failed to create SDL window.");
  }

  context.sdlWindow = window;

  Window::SetSettings(wcontext, wcontext.initialSettings);

  // Get vulkan instance extensions required by SDL
  unsigned int extensionCount = 0;
  SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  if (extensionCount == 0) {
    return Error::Create("Failed to get Vulkan instance extension count.");
  }

  Uint32 extCount = 0;
  const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extCount);
  if (extensions == nullptr) {
    return Error::Create("Failed to get Vulkan instance extensions.");
  }

  std::vector<const char *> extensionList;

  extensionList.reserve(extCount);
  for (Uint32 i = 0; i < extCount; i++) {
    extensionList.emplace_back(extensions[i]); // NOLINT
  }

  extensionList.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Thallium Engine";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Thallium";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_4;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(extensionList.size());
  createInfo.ppEnabledExtensionNames = extensionList.data();
  createInfo.pApplicationInfo = &appInfo;

  error =
      Error::Create(vkCreateInstance(&createInfo, nullptr, &context.instance));

  if (Error::IsError(error)) {
    return error;
  }

  PrintDebug("Loading Vulkan instance with Volk...");
  // Load instance-level Vulkan functions using Volk
  volkLoadInstance(context.instance);

  // Create Vulkan surface for the SDL window
  if (!SDL_Vulkan_CreateSurface(window, context.instance, nullptr,
                                &context.surface)) {
    return Error::Create("Failed to create Vulkan surface.");
  }

  error = FindPhysicalDevice(context);
  if (Error::IsError(error)) {
    return error;
  }

  auto *windowContext = Window::GetWindowContext();

  if (windowContext == nullptr) {
    return Error::Create("No current window context found.");
  }

  error = CreateDevice(context);
  if (Error::IsError(error)) {
    return error;
  }
  PrintDebug("called: CreateDevice...");
  error = CreateVmaAllocator(context);
  if (Error::IsError(error)) {
    return error;
  }
  PrintDebug("called: CreateVmaAllocator...");

  GetThreadContext().graphicsContext = &context;

  error = CreateCommandPool(GetThreadContext());
  if (Error::IsError(error)) {
    return error;
  }
  PrintDebug("called: CreateCommandPool...");
  error = CreateSwapchain(context, *windowContext);
  if (Error::IsError(error)) {
    return error;
  }
  PrintDebug("called: CreateSwapchain...");
  error = CreateSemaphores(context);
  if (Error::IsError(error)) {
    return error;
  }
  PrintDebug("called: CreateSemaphores...");
  error = CreateFences(context);
  if (Error::IsError(error)) {
    return error;
  }
  PrintDebug("called: CreateFences.");

  return Error::Success();
}

void Deinitialize(GraphicsContext &context) {
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  std::lock_guard<std::mutex> lock2(
      Graphics::GraphicsContext::mutexes.vmaAllocator);
  vkDeviceWaitIdle(context.device);

  vmaDestroyAllocator(context.vmaAllocator);

  for (VkFence fence : context.inFlightFences) {
    vkDestroyFence(context.device, fence, nullptr);
  }

  for (VkSemaphore semaphore : context.renderingFinished) {
    vkDestroySemaphore(context.device, semaphore, nullptr);
  }

  for (VkSemaphore semaphore : context.swapchainImageReady) {
    vkDestroySemaphore(context.device, semaphore, nullptr);
  }

  vkDestroyCommandPool(context.device, GetThreadContext().commandPool, nullptr);
  {
    std::lock_guard<std::mutex> lock(CommandPoolsMutex);
    for (auto &pool : CommandPools) {
      vkDestroyCommandPool(context.device, pool, nullptr);
    }
  }

  for (VkImageView imageView : context.swapchainInfo.imageViews) {
    vkDestroyImageView(context.device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(context.device, context.swapchainInfo.swapchain,
                        nullptr);

  vkDestroyDevice(context.device, nullptr);
  vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
  vkDestroyInstance(context.instance, nullptr);

  SDL_DestroyWindow(context.sdlWindow);
  SDL_Quit();
}

auto BeginSingleTimeCommands(GraphicsContext &context) -> VkCommandBuffer {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  auto &tcontext = GetThreadContext();

  allocInfo.commandPool = tcontext.commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer = nullptr;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
  }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

auto EndSingleTimeCommands(GraphicsContext &context,
                           VkCommandBuffer commandBuffer) -> void {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context.graphicsQueue);

  auto &tcontext = GetThreadContext();

  vkFreeCommandBuffers(context.device, tcontext.commandPool, 1, &commandBuffer);
}

void SetCurrentGraphicsContext(GraphicsContext *ctx) { g_ctx = ctx; }
auto GetCurrentGraphicsContext() -> GraphicsContext * { return g_ctx; }

} // namespace Graphics