#include "imgui.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
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
    Graphics::Rendergraph::ResourceHandle lastResourceHandle, // NOLINT
    Graphics::Rendergraph::ResourceHandle writeResourceHandle,
    Context &editorContext) -> Error::Error {
  // Initialize ImGui context

  std::cout << "Creating shaders for ImGui..." << "\n";

  auto vertexShader = Graphics::Shader::ShaderModule::Create(
      context, "src/Graphics/Shaders/imgui.vs", VK_SHADER_STAGE_VERTEX_BIT,
      "ImGui vertex shader");

  if (Error::IsError(vertexShader)) {
    return vertexShader.error();
  }

  auto fragmentShader = Graphics::Shader::ShaderModule::Create(
      context, "src/Graphics/Shaders/imgui.fs", VK_SHADER_STAGE_FRAGMENT_BIT,
      "ImGui fragment shader");

  if (Error::IsError(fragmentShader)) {
    return fragmentShader.error();
  }

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

  init_info.Allocator = nullptr;
  init_info.UseDynamicRendering = VK_TRUE;
  init_info.PipelineInfoMain = {
      .Subpass = 0,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
  };

  std::cout << "Initializing ImGui Vulkan backend..." << "\n";

  PFN_vkGetDeviceProcAddr func = vkGetDeviceProcAddr;
  std::cout << "vkGetDeviceProcAddr = " << (void *)func << "\n";

  auto load_vk_func = [&](const char *func) -> auto {
    if (auto proc = vkGetDeviceProcAddr(context.device, func)) {
      return proc;
    }
    return vkGetInstanceProcAddr(context.instance, func);
  };
  ImGui_ImplVulkan_LoadFunctions(
      VK_API_VERSION_1_4,
      [](const char *func, void *data) -> PFN_vkVoidFunction {
        // NOLINTNEXTLINE
        return (*(decltype(load_vk_func) *)data)(func);
      },
      &load_vk_func);

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
      {
          .resources = {{
                            .resource = lastResourceHandle,
                            .accessType =
                                Graphics::Rendergraph::AccessType::Read,
                        },
                        {
                            .resource = writeResourceHandle,
                            .accessType =
                                Graphics::Rendergraph::AccessType::Write,
                        }},
          .viewport = {.x = 0.0F,
                       .y = 0.0F,
                       .width = static_cast<float>(
                           context.swapchainInfo.extent.width),
                       .height = static_cast<float>(
                           context.swapchainInfo.extent.height),
                       .minDepth = 0.0F,
                       .maxDepth = 1.0F},
          .resourceBindings =
              {{
                   .resource = writeResourceHandle,
                   .location = 0,
                   .type = Graphics::Rendergraph::BindingType::Attachment,
               },
               {
                   .resource = lastResourceHandle,
                   .binding = 0,
                   .set = 0,
                   .location = 1,
                   .type = Graphics::Rendergraph::BindingType::Sampler,
               }},
          .vertexShader = vertexShader.value(),
          .fragmentShader = fragmentShader.value(),
          .executeFunction =
              [](VkCommandBuffer cmd, Graphics::GraphicsContext &context,
                 Graphics::Rendergraph::RenderGraph &graph) -> void {
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
          },
      });

  return Error::Success();
}
} // namespace Editor