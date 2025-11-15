#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES

#include "imgui.hpp"
#include "Graphics/graphics.hpp"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <iostream>
#include <print>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "volk/volk.h"

void CheckVk(VkResult err) {
  if (err != VK_SUCCESS) {
    std::println("Vulkan error: {}", static_cast<int>(err));
    abort();
  }
}

namespace Editor {

auto InitializeImGui(
    Graphics::GraphicsContext &context,
    Graphics::Rendergraph::RenderGraph &graph,
    Graphics::Rendergraph::ResourceHandle lastPassHandle, // NOLINT
    Graphics::Rendergraph::ResourceHandle writeResourceHandle,
    Context &editorContext) -> Error::Error {
  // Initialize ImGui context

  std::cout << "Initializing ImGui..." << "\n";

  IMGUI_CHECKVERSION();

  std::cout << "Creating ImGui context..." << "\n";
  editorContext.imguiContext = ImGui::CreateContext();

  std::cout << "Setting ImGui IO..." << "\n";
  ImGuiIO &inputOutput = ImGui::GetIO();
  (void)inputOutput;

  std::cout << "Setting ImGui style..." << "\n";
  // Setup ImGui style
  ImGui::StyleColorsDark();

  std::cout << "Initializing ImGui SDL3 backend..." << "\n";

  auto success = ImGui_ImplSDL3_InitForVulkan(context.sdlWindow);

  if (!success) {
    return Error::Create("Failed to initialize ImGui SDL3 backend.");
  }

  // Setup Platform/Renderer backends
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.ApiVersion = VK_API_VERSION_1_4;
  init_info.Instance = context.instance;
  init_info.PhysicalDevice = context.physicalDevice;
  init_info.Device = context.device;
  init_info.QueueFamily = context.graphicsQueueFamily;
  init_info.Queue = context.graphicsQueue;
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = context.descriptorPool;
  init_info.MinImageCount = context.swapchainInfo.imageCount;
  init_info.ImageCount = context.swapchainInfo.imageCount;

  std::cout << "Setting ImGui Vulkan init info..." << "\n";
  std::cout << "Instance: " << init_info.Instance << "\n";
  std::cout << "PhysicalDevice: " << init_info.PhysicalDevice << "\n";
  std::cout << "Device: " << init_info.Device << "\n";
  std::cout << "QueueFamily: " << init_info.QueueFamily << "\n";
  std::cout << "Queue: " << init_info.Queue << "\n";
  std::cout << "DescriptorPool: " << init_info.DescriptorPool << "\n";
  std::cout << "MinImageCount: " << init_info.MinImageCount << "\n";
  std::cout << "ImageCount: " << init_info.ImageCount << "\n";

  init_info.Allocator = nullptr;
  init_info.UseDynamicRendering = VK_TRUE;
  init_info.PipelineInfoMain = {
      .Subpass = 0,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
  };

  std::cout << "Initializing ImGui Vulkan backend..." << "\n";

  PFN_vkGetDeviceProcAddr func = vkGetDeviceProcAddr;
  std::cout << "vkGetDeviceProcAddr = " << (void *)func << "\n";

  success = ImGui_ImplVulkan_LoadFunctions(
      VK_API_VERSION_1_4,
      [](const char *function_name,
         void *vulkan_instance) -> PFN_vkVoidFunction {
        return vkGetInstanceProcAddr(
            // NOLINTNEXTLINE
            *(reinterpret_cast<VkInstance *>(vulkan_instance)), function_name);
      },
      context.instance);

  if (!success) {
    return Error::Create("Failed to load ImGui Vulkan functions.");
  }

  success = ImGui_ImplVulkan_Init(&init_info);
  if (!success) {
    return Error::Create("Failed to initialize ImGui Vulkan backend.");
  }

  std::cout << "Creating ImGui fonts texture..." << "\n";

  // Create render pass for ImGui
  Graphics::Rendergraph::AddRenderPass(
      graph,
      {.resources =
           {
               {
                   .resource = lastPassHandle,
                   .accessType = Graphics::Rendergraph::AccessType::Write,
               },
           },

       .viewport = {.x = 0.0F,
                    .y = 0.0F,
                    .width =
                        static_cast<float>(context.swapchainInfo.extent.width),
                    .height =
                        static_cast<float>(context.swapchainInfo.extent.height),
                    .minDepth = 0.0F,
                    .maxDepth = 1.0F},
       .executeFunction =
           [](VkCommandBuffer cmd, Graphics::GraphicsContext &context,
              Graphics::Rendergraph::RenderGraph &graph) -> void {
         ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
       }});

  return Error::Success();
}
} // namespace Editor