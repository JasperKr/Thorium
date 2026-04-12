#include "swapchainManager.hpp"

#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/utils.hpp"
#include "Modules/window.hpp"
#include <public/tracy/Tracy.hpp>

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <vector>
namespace Graphics::SwapchainManager {

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

inline auto FindPresentMode(Window::WindowContext &wcontext,
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

inline auto
FindHDRColorSpaceSupport(const GraphicsContext &context,
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
FindSRGBColorSpaceSupport(const GraphicsContext &context,
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
FindLinearColorSpaceSupport(const GraphicsContext &context,
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

inline auto FindSurfaceFormat(Window::ColorSpace colorspace,
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
    return FindSRGBColorSpaceSupport(context, formats);
  case Window::ColorSpace::Linear:
    return FindLinearColorSpaceSupport(context, formats);
  case Window::ColorSpace::HDR: {
    auto result = FindHDRColorSpaceSupport(context, formats);
    if (Error::IsError(result)) {
      PrintWarning(
          "HDR color space requested but not supported. Falling back to sRGB.");
      return FindSRGBColorSpaceSupport(context, formats);
    }

    return result;
  }
  default:
    return Error::Unexpected("No suitable surface format found", -2);
  }
}

inline auto GetSwapchainTextures(GraphicsContext &context) -> Error {
  context.swapchainInfo.textures.clear();
  context.swapchainInfo.textures.resize(context.swapchainInfo.imageCount);

  for (uint32_t i = 0; i < context.swapchainInfo.imageCount; i++) {
    auto textureResult = Graphics::FromSwapchainTexture(
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

auto SwapchainManager::Initialize(GraphicsContext &context,
                                  Window::WindowContext &windowContext)
    -> Result<SwapchainManager> {
  SwapchainManager manager;

  auto result = manager.CreateVkSwapchain(context, windowContext);
  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  manager.currentSwapchain = context.swapchainInfo.swapchain;
  manager.currentTextures = context.swapchainInfo.textures;
  manager.lastFrameUsed = 0;
  manager.isDirty = false;

  return manager;
}

auto SwapchainManager::Deinitialize(GraphicsContext &context) -> void {
  for (auto &texture : currentTextures) {
    auto *image = texture->image;
    auto *view = texture->view;

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      vkDestroyImageView(context.device, view, nullptr);
      // We do not destroy the image here because it is owned by the swapchain.
    }

    texture->image = nullptr;
    texture->view = VK_NULL_HANDLE;
  }

  currentTextures.clear();

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkDestroySwapchainKHR(context.device, currentSwapchain, nullptr);
  }

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    for (auto &fence : context.imageInFlight) {
      vkDestroyFence(context.device, fence, nullptr);
    }

    for (auto &semaphore : context.imageReady) {
      vkDestroySemaphore(context.device, semaphore, nullptr);
    }

    context.imageInFlight.clear();
    context.imageReady.clear();
  }

  currentSwapchain = VK_NULL_HANDLE;

  CleanupOldSwapchains(context, UINT64_MAX);
}
auto SwapchainManager::RecreateSwapchain(GraphicsContext &context,
                                         Window::WindowContext &wcontext)
    -> Error {
  ZoneScoped;

  OldSwapchain oldSwapchain{
      .swapchain = currentSwapchain,
      .textures = currentTextures,
      .lastFrameUsed = lastFrameUsed,
      .imageReady = context.imageReady,
      .imageInFlight = context.imageInFlight,
  };

  oldSwapchains.emplace_back(oldSwapchain);

  context.imageReady.clear();
  context.imageInFlight.clear();

  auto createResult = CreateVkSwapchain(context, wcontext);
  if (Error::IsError(createResult)) {
    return createResult;
  }

  currentSwapchain = context.swapchainInfo.swapchain;
  currentTextures = context.swapchainInfo.textures;
  isDirty = false;

  context.frameIndex = 0;

  return Error::Success();
}
auto SwapchainManager::CleanupOldSwapchains(GraphicsContext &context,
                                            uint64_t currentFrame) -> void {
  Utils::UnorderedErase(
      oldSwapchains,
      [&context, &currentFrame](OldSwapchain &oldSwapchain) -> bool {
        auto swapchainImageCount = oldSwapchain.textures.size();
        if (currentFrame - oldSwapchain.lastFrameUsed >
            swapchainImageCount * 2) {
          for (auto &texture : oldSwapchain.textures) {
            auto *image = texture->image;
            auto *view = texture->view;

            {
              std::lock_guard<std::mutex> lock(
                  Graphics::GraphicsContext::mutexes.device);
              vkDestroyImageView(context.device, view, nullptr);
            }

            texture->image = nullptr;
            texture->view = VK_NULL_HANDLE;
          }

          oldSwapchain.textures.clear();

          std::lock_guard<std::mutex> lock(
              Graphics::GraphicsContext::mutexes.device);
          vkDestroySwapchainKHR(context.device, oldSwapchain.swapchain,
                                nullptr);
          oldSwapchain.swapchain = VK_NULL_HANDLE;

          for (auto &fence : oldSwapchain.imageInFlight) {
            vkDestroyFence(context.device, fence, nullptr);
          }

          for (auto &semaphore : oldSwapchain.imageReady) {
            vkDestroySemaphore(context.device, semaphore, nullptr);
          }

          oldSwapchain.imageInFlight.clear();
          oldSwapchain.imageReady.clear();

          return true;
        }

        return false;
      });
}

auto SwapchainManager::GetCurrentSwapchain() -> VkSwapchainKHR {
  return currentSwapchain;
}
auto SwapchainManager::GetCurrentSwapchainTexture(
    const GraphicsContext &context) -> Ref<Texture> {
  return currentTextures.at(context.swapchainImageIndex);
}

auto SwapchainManager::NewFrame(GraphicsContext &context,
                                Window::WindowContext &windowContext,
                                uint64_t currentFrame) -> Error {
  ZoneScoped;

  if (isDirty) {
    isDirty = false;

    auto result = RecreateSwapchain(context, windowContext);
    if (Error::IsError(result)) {
      return result;
    }
  }

  CleanupOldSwapchains(context, currentFrame);

  lastFrameUsed = currentFrame;

  return Error::Success();
}
auto SwapchainManager::EndFrame(const GraphicsContext &context) -> Error {
  ZoneScoped;
  auto error = GetCurrentSwapchainTexture(context)->UseAsPresentSrc(context);
  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

inline auto CreateFences(GraphicsContext &context) -> Error {
  ZoneScoped;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  context.imageInFlight.resize(MAX_SWAPCHAIN_IMAGES);
  context.imageReady.resize(MAX_SWAPCHAIN_IMAGES);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    for (int i = 0; i < MAX_SWAPCHAIN_IMAGES; i++) {
      Error error = Error::Create(vkCreateFence(
          context.device, &fenceInfo, nullptr, &context.imageInFlight.at(i)));
      if (Error::IsError(error)) {
        return error;
      }

      error = Error::Create(vkCreateSemaphore(
          context.device, &semaphoreInfo, nullptr, &context.imageReady.at(i)));
      if (Error::IsError(error)) {
        return error;
      }
    }
  }

  if (context.swapchainInfo.imageCount <= 0) {
    return Error::Create("Swapchain image count is zero.");
  }

  return Error::Success();
}

auto SwapchainManager::CreateVkSwapchain(GraphicsContext &context,
                                         Window::WindowContext &windowContext)
    -> Error {
  ZoneScoped;

  auto presentResult = FindPresentMode(windowContext, context);
  if (Error::IsError(presentResult)) {
    return presentResult.error();
  }
  VkPresentModeKHR presentMode = presentResult.value();

  context.surfaceInfo.presentMode = presentMode;

  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      context.physicalDevice, context.surface, &surfaceCapabilities);

  auto surfaceResult = FindSurfaceFormat(windowContext.colorSpace, context);
  if (Error::IsError(surfaceResult)) {
    return surfaceResult.error();
  }
  VkSurfaceFormatKHR surfaceFormat = surfaceResult.value();

  context.surfaceInfo.format = surfaceFormat;
  context.surfaceInfo.capabilities = surfaceCapabilities;

  VkSwapchainCreateInfoKHR swapchainInfo = {};
  swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainInfo.surface = context.surface;

  swapchainInfo.minImageCount = surfaceCapabilities.minImageCount;

  if (surfaceCapabilities.maxImageCount > 0 &&
      swapchainInfo.minImageCount > surfaceCapabilities.maxImageCount) {
    swapchainInfo.minImageCount = surfaceCapabilities.maxImageCount;
  }

  int width = 0;
  int height = 0;
  SDL_GetWindowSizeInPixels(context.sdlWindow, &width, &height);

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
  swapchainInfo.oldSwapchain = currentSwapchain;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    Error error = Error::Create(vkCreateSwapchainKHR(
        context.device, &swapchainInfo, nullptr, &currentSwapchain));

    if (Error::IsError(error)) {
      return error;
    }
  }

  context.swapchainInfo.swapchain = currentSwapchain;
  context.swapchainInfo.format = surfaceFormat.format;
  context.swapchainInfo.extent = swapchainInfo.imageExtent;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkGetSwapchainImagesKHR(context.device, currentSwapchain,
                            &context.swapchainInfo.imageCount, nullptr);
  }

  context.swapchainInfo.images.resize(context.swapchainInfo.imageCount);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    Error error = Error::Create(vkGetSwapchainImagesKHR(
        context.device, currentSwapchain, &context.swapchainInfo.imageCount,
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

  if (context.imageReady.empty() ||
      context.imageReady.at(0) == VK_NULL_HANDLE) {
    result = CreateFences(context);
    if (Error::IsError(result)) {
      return result;
    }
  }

  return Error::Success();
}

auto SwapchainManager::MakeDirty() -> void { isDirty = true; }

} // namespace Graphics::SwapchainManager