#include "swapchainManager.hpp"

#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"

#define VK_NO_PROTOTYPES
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
  case Window::ColorSpace::HDR:
    return FindHDRColorSpaceSupport(context, formats);
  default:
    return Error::Unexpected("No suitable surface format found", -2);
  }
}

inline auto GetSwapchainTextures(GraphicsContext &context) -> Error {
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

auto SwapchainManager::Initialize(GraphicsContext &context,
                                  Window::WindowContext &windowContext)
    -> Error {

  auto result = CreateVkSwapchain(context, windowContext);
  if (Error::IsError(result)) {
    return result;
  }

  return Error::Success();
}

auto SwapchainManager::Deinitialize(GraphicsContext &context) -> void {}
auto SwapchainManager::RecreateSwapchain(GraphicsContext &context,
                                         Window::WindowContext &wcontext)
    -> Error {
  OldSwapchain oldSwapchain{
      .swapchain = currentSwapchain,
      .textures = currentTextures,
      .lastFrameUsed = lastFrameUsed,
  };

  oldSwapchains.emplace_back(oldSwapchain);

  return CreateVkSwapchain(context, wcontext);
}
auto SwapchainManager::CleanupOldSwapchains(GraphicsContext &context,
                                            uint64_t currentFrame) -> void {
  for (auto &oldSwapchain : oldSwapchains) {
    auto swapchainImageCount = oldSwapchain.textures.size();
    if (currentFrame - oldSwapchain.lastFrameUsed > swapchainImageCount + 1) {
      for (auto &texture : oldSwapchain.textures) {
        auto *image = texture->image;
        auto *view = texture->view;

        {
          std::lock_guard<std::mutex> lock(
              Graphics::GraphicsContext::mutexes.device);
          vkDestroyImageView(context.device, view, nullptr);
        }
      }

      oldSwapchain.textures.clear();

      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      vkDestroySwapchainKHR(context.device, oldSwapchain.swapchain, nullptr);
    }
  }
}
auto SwapchainManager::GetCurrentSwapchain() -> VkSwapchainKHR {
  return currentSwapchain;
}
auto SwapchainManager::GetCurrentSwapchainTexture(
    const GraphicsContext &context) -> Ref<Texture::Texture> {
  return currentTextures.at(context.swapchainImageIndex);
}

auto SwapchainManager::NewFrame(GraphicsContext &context,
                                Window::WindowContext &windowContext,
                                uint64_t currentFrame) -> Error {
  if (isDirty) {
    isDirty = false;

    auto result = RecreateSwapchain(context, windowContext);
    if (Error::IsError(result)) {
      return result;
    }
  }

  lastFrameUsed = currentFrame;

  return Error::Success();
}
auto SwapchainManager::EndFrame(const GraphicsContext &context) -> Error {
  auto error = GetCurrentSwapchainTexture(context)->UseAsPresentSrc(context);
  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

auto SwapchainManager::CreateVkSwapchain(GraphicsContext &context,
                                         Window::WindowContext &windowContext)
    -> Error {

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

  return Error::Success();
}

} // namespace Graphics::SwapchainManager