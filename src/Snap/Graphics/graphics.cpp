#include "graphics.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/deviceSettings.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/window.hpp"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"

#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
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

auto GetAvailableDeviceExtensions(const GraphicsContext &context)
    -> Result<std::vector<VkExtensionProperties>> {
  uint32_t extensionCount = 0;
  auto error = Error::Create(vkEnumerateDeviceExtensionProperties(
      context.physicalDevice, nullptr, &extensionCount, nullptr));

  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  std::vector<VkExtensionProperties> extensions(extensionCount);
  error = Error::Create(vkEnumerateDeviceExtensionProperties(
      context.physicalDevice, nullptr, &extensionCount, extensions.data()));

  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  return extensions;
}

auto ExtensionListSupported(
    const std::vector<std::pair<const char *, ExtensionRequirement>>
        &extensions,
    const std::vector<VkExtensionProperties> &availableExtensions)
    -> std::vector<bool> {
  std::vector<bool> supported(extensions.size(), false);

  for (size_t i = 0; i < extensions.size(); i++) {
    for (const auto &available : availableExtensions) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay, hicpp-no-array-decay)
      if (strcmp(extensions.at(i).first, available.extensionName) == 0) {
        supported.at(i) = true;
        break;
      }
    }
  }

  return supported;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static auto CreateDevice(GraphicsContext &context,
                         const DeviceSettings &settings) -> Error {
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

  VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT feature{};
  feature.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
  feature.vertexInputDynamicState = VK_TRUE;

  // --- Vulkan 1.3 features ---
  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &feature,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE,
  };

  PrintDebug("Enabled Vulkan 1.3 features: synchronization2, dynamicRendering");

  // --- Vulkan 1.2 features ---
  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &features13,
      .shaderFloat16 = VK_TRUE,
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

  // --- Features2 root ---
  VkPhysicalDeviceFeatures2 features2{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &features11,
  };

  // Device create info
  createInfo.pNext = &features2;
  createInfo.pEnabledFeatures = nullptr;

  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  constexpr auto extDisabled = ExtensionRequirement::Disabled;
  constexpr auto extOptional = ExtensionRequirement::Optional;
  constexpr auto extRequired = ExtensionRequirement::Required;

  const auto &result = GetAvailableDeviceExtensions(context);
  if (Error::IsError(result)) {
    return result.error();
  }
  const auto &availableExtensions = result.value();

  auto extensions = std::vector<std::pair<const char *, ExtensionRequirement>>{
      {VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, extOptional},
      {VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, extRequired},
      {VK_KHR_SPIRV_1_4_EXTENSION_NAME, extRequired},
      {VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, extRequired},
      {VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, extRequired},
      {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extOptional},
      {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
       settings.hardwareRaytracing},
      {VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, settings.hardwareRaytracing},
      {VK_KHR_RAY_QUERY_EXTENSION_NAME, settings.inlineRaytracing},
  };

  const auto &supported =
      ExtensionListSupported(extensions, availableExtensions);
  bool allRequiredSupported = true;
  auto index = 0;

  for (const auto &[extensionName, requirement] : extensions) {
    if (supported.at(index) && requirement != extDisabled) {
      deviceExtensions.emplace_back(extensionName);
    } else {
      if (requirement == extRequired) {
        return Error::Create(std::format(
            "Required device extension not supported: {}", extensionName));
      }

      if (requirement == extOptional) {
        PrintWarning("Optional device extension not supported: {}",
                     extensionName);
      } else {
        PrintInfo("Device extension not supported (disabled): {}",
                  extensionName);
      }
    }
    index++;
  }

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  Error error =
      Error::Create(vkCreateDevice(context.physicalDevice, &createInfo,
                                   GetAllocationCallbacks(), &context.device));

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
  thread_local ThreadContext threadContext;
  return threadContext;
}

// May be null
auto GetCommandBuffer() -> VkCommandBuffer {
  auto &threadContext = GetThreadContext();
  return threadContext.commandBuffer;
}

static auto CreateSemaphores(GraphicsContext &context) -> Error {
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  context.imageAvailable.resize(FRAMES_IN_FLIGHT);
  context.inFlight.resize(FRAMES_IN_FLIGHT);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
      Error error = Error::Create(vkCreateSemaphore(
          context.device, &semaphoreInfo, GetAllocationCallbacks(),
          &context.imageAvailable.at(i)));
      if (Error::IsError(error)) {
        return error;
      }

      error = Error::Create(vkCreateFence(context.device, &fenceInfo,
                                          GetAllocationCallbacks(),
                                          &context.inFlight.at(i)));
      if (Error::IsError(error)) {
        return error;
      }
    }
  }

  return Error::Success();
}

static auto CreateVmaAllocator(GraphicsContext &context) -> Error {
  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  VmaAllocatorCreateInfo allocatorInfo = {0};
  allocatorInfo.physicalDevice = context.physicalDevice;
  allocatorInfo.device = context.device;
  allocatorInfo.instance = context.instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
  allocatorInfo.pAllocationCallbacks = GetAllocationCallbacks();
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
                            GetAllocationCallbacks(), &tcontext.commandPool));

    if (Error::IsError(error)) {
      return error;
    }
  }

  {
    std::lock_guard<std::mutex> lock(CommandPoolsMutex);
    CommandPools.emplace_back(tcontext.commandPool);
  }

  return Error::Success();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Initialize(GraphicsContext &context, Window::WindowContext &wcontext,
                const DeviceSettings &deviceSettings) -> Error {

  GlobalAllocations.RegisterNewThreadAllocations();

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
  appInfo.pApplicationName = "Snap Engine";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Snap";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_4;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(extensionList.size());
  createInfo.ppEnabledExtensionNames = extensionList.data();
  createInfo.pApplicationInfo = &appInfo;

  error = Error::Create(vkCreateInstance(&createInfo, GetAllocationCallbacks(),
                                         &context.instance));

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

  error = CreateDevice(context, deviceSettings);
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
  error = CreateSemaphores(context);
  if (Error::IsError(error)) {
    return error;
  }
  PrintDebug("called: CreateSemaphores...");

  return Error::Success();
}

void Deinitialize(GraphicsContext &context) {
  PrintInfo("Deinitializing graphics context...");

  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  vkDeviceWaitIdle(context.device);

  for (auto &descriptorPoolInfo : GetThreadContext().descriptorPools) {
    vkDestroyDescriptorPool(context.device, descriptorPoolInfo.descriptorPool,
                            GetAllocationCallbacks());
  }

  vmaDestroyAllocator(context.vmaAllocator);

  for (VkSemaphore semaphore : context.imageAvailable) {
    vkDestroySemaphore(context.device, semaphore, GetAllocationCallbacks());
  }

  for (VkFence fence : context.inFlight) {
    vkDestroyFence(context.device, fence, GetAllocationCallbacks());
  }

  {
    std::lock_guard<std::mutex> lock(CommandPoolsMutex);
    for (auto &pool : CommandPools) {
      vkDestroyCommandPool(context.device, pool, GetAllocationCallbacks());
    }
    CommandPools.clear();
  }

  vkDestroyDevice(context.device, GetAllocationCallbacks());
  vkDestroySurfaceKHR(context.instance, context.surface,
                      GetAllocationCallbacks());
  vkDestroyInstance(context.instance, GetAllocationCallbacks());

  SDL_DestroyWindow(context.sdlWindow);
  SDL_Quit();

  context.sdlWindow = nullptr;
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