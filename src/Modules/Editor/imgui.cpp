#include "imgui.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/rendergraph.hpp"
#include "Graphics/shader.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <print>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "volk/volk.h"

void CheckVk(VkResult err) {
  if (err != VK_SUCCESS) {
    PrintError("Vulkan error: {}", static_cast<int>(err));
    abort();
  }
}

static void ImGui_ImplVulkan_SetupRenderState(
    ImDrawData *draw_data, VkPipeline pipeline, VkCommandBuffer command_buffer,
    std::pair<Ref<Graphics::Buffer>, Ref<Graphics::Buffer>> &renderBuffers,
    int fb_width, // NOLINT
    int fb_height, Graphics::Rendergraph::CompiledPass &currentPass) {

  // Bind pipeline:
  {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);
  }

  // Bind Vertex And Index Buffer:
  if (draw_data->TotalVtxCount > 0) {
    std::array<VkBuffer, 1> vertex_buffers = {renderBuffers.first->handle};
    std::array<VkDeviceSize, 1> vertex_offset = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers.data(),
                           vertex_offset.data());
    vkCmdBindIndexBuffer(command_buffer, renderBuffers.second->handle, 0,
                         sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16
                                                : VK_INDEX_TYPE_UINT32);
  }

  // Setup viewport:
  {
    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)fb_width;
    viewport.height = (float)fb_height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
  }

  // Setup scale and translation:
  // Our visible imgui space lies from draw_data->DisplayPps (top left) to
  // draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is
  // (0,0) for single viewport apps.
  {
    // float scale[2];
    std::array<float, 2> scale{};
    scale[0] = 2.0F / draw_data->DisplaySize.x; // NOLINT
    scale[1] = 2.0F / draw_data->DisplaySize.y; // NOLINT
    // float translate[2];
    std::array<float, 2> translate{};
    translate[0] = -1.0F - (draw_data->DisplayPos.x * scale[0]);
    translate[1] = -1.0F - (draw_data->DisplayPos.y * scale[1]);
    vkCmdPushConstants(command_buffer, currentPass.pass.state.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0,
                       sizeof(float) * 2, scale.data());
    vkCmdPushConstants(command_buffer, currentPass.pass.state.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2,
                       sizeof(float) * 2, translate.data());
  }
}

namespace Editor {

auto GetMeshBuffers()
    -> std::pair<Ref<Graphics::Buffer>, Ref<Graphics::Buffer>> {
  static auto vertexBuffer = Ref<Graphics::Buffer>::Make();
  static auto indexBuffer = Ref<Graphics::Buffer>::Make();

  return {vertexBuffer, indexBuffer};
}

// NOLINTNEXTLINE
auto RenderImguiDrawLists(VkCommandBuffer commandBuffer,
                          Graphics::GraphicsContext &context,
                          Graphics::Rendergraph::RenderGraph &graph,
                          Graphics::Rendergraph::CompiledPass &currentPass)
    -> Error::Error {
  ImDrawData *draw_data = ImGui::GetDrawData();

  // Avoid rendering when minimized, scale coordinates for retina displays
  // (screen coordinates != framebuffer coordinates)
  int fb_width =
      (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
  int fb_height =
      (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
  if (fb_width <= 0 || fb_height <= 0) {
    return Error::Success();
  }

  // Catch up with texture updates. Most of the times, the list will have 1
  // element with an OK status, aka nothing to do. (This almost always points to
  // ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow
  // overriding or disabling texture updates).
  if (draw_data->Textures != nullptr) {
    for (ImTextureData *tex : *draw_data->Textures) {
      if (tex->Status != ImTextureStatus_OK) {
        ImGui_ImplVulkan_UpdateTexture(tex);
      }
    }
  }

  // Allocate array to store enough vertex/index buffers
  auto buffers = GetMeshBuffers();
  auto &vertexBuffer = buffers.first;
  auto &indexBuffer = buffers.second;

  if (draw_data->TotalVtxCount > 0) {
    // Create or resize the vertex/index buffers
    VkDeviceSize vertexBufferSize =
        draw_data->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    if (vertexBuffer->handle == VK_NULL_HANDLE) {
      auto vtxBufferResult = Graphics::Buffer::Create(
          context,
          Graphics::BufferCreationInfo{
              .size = vertexBufferSize,
              .usage =
                  static_cast<uint32_t>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
                  static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
              .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

      if (Error::IsError(vtxBufferResult)) {
        return vtxBufferResult.error();
      }

      vertexBuffer = vtxBufferResult.value();
    }

    if (indexBuffer->handle == VK_NULL_HANDLE) {
      auto idxBufferResult = Graphics::Buffer::Create(
          context,
          Graphics::BufferCreationInfo{
              .size = indexBufferSize,
              .usage = static_cast<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) |
                       static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
              .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

      if (Error::IsError(idxBufferResult)) {
        return idxBufferResult.error();
      }

      indexBuffer = idxBufferResult.value();
    }

    // if (vertexBuffer->size < vertexBufferSize) {
    //   auto resizeResult = vertexBuffer->Resize(context, vertexBufferSize);
    //   if (Error::IsError(resizeResult)) {
    //     return resizeResult;
    //   }
    // }

    // if (indexBuffer->size < indexBufferSize) {
    //   auto resizeResult = indexBuffer->Resize(context, indexBufferSize);
    //   if (Error::IsError(resizeResult)) {
    //     return resizeResult;
    //   }
    // }
  }

  // Setup render state structure (for callbacks and custom texture bindings)
  ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
  ImGui_ImplVulkan_RenderState render_state{};
  render_state.CommandBuffer = commandBuffer;
  render_state.Pipeline = currentPass.pass.state.pipeline;
  render_state.PipelineLayout = currentPass.pass.state.pipelineLayout;
  platform_io.Renderer_RenderState = &render_state;

  // Will project scissor/clipping rectangles into framebuffer space
  ImVec2 clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
  ImVec2 clip_scale =
      draw_data->FramebufferScale; // (1,1) unless using retina display which
                                   // are often (2,2)

  // Render command lists
  // (Because we merged all buffers into a single one, we maintain our own
  // offset into them)
  VkDescriptorSet last_desc_set = VK_NULL_HANDLE;
  uint32_t global_vtx_offset = 0;
  uint32_t global_idx_offset = 0;
  for (const ImDrawList *draw_list : draw_data->CmdLists) {
    for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd *pcmd = &draw_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback != nullptr) {
        // User callback, registered via ImDrawList::AddCallback()
        // (ImDrawCallback_ResetRenderState is a special callback value used by
        // the user to request the renderer to reset render state.)
        if (pcmd->UserCallback == ImDrawCallback_ResetRenderState) // NOLINT
          ImGui_ImplVulkan_SetupRenderState(
              draw_data, currentPass.pass.state.pipeline, commandBuffer,
              buffers, fb_width, fb_height, currentPass);
        else {
          pcmd->UserCallback(draw_list, pcmd);
        }
        last_desc_set = VK_NULL_HANDLE;
      } else {
        // Project scissor/clipping rectangles into framebuffer space
        ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x,
                        (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
        ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x,
                        (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

        // Clamp to viewport as vkCmdSetScissor() won't accept values that are
        // off bounds
        clip_min.x = (std::max)(clip_min.x, 0.0F);
        clip_min.y = (std::max)(clip_min.y, 0.0F);
        clip_max.x = (std::min)(clip_max.x, static_cast<float>(fb_width));
        clip_max.y = (std::min)(clip_max.y, static_cast<float>(fb_height));
        if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
          continue;
        }

        // Apply scissor/clipping rectangle
        VkRect2D scissor;
        scissor.offset.x = (int32_t)(clip_min.x);
        scissor.offset.y = (int32_t)(clip_min.y);
        scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
        scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Bind DescriptorSet with font or user texture NOLINTNEXTLINE
        auto *desc_set = (VkDescriptorSet)pcmd->GetTexID();
        if (desc_set != last_desc_set) {
          // vkCmdBindDescriptorSets(commandBuffer,
          //                         VK_PIPELINE_BIND_POINT_GRAPHICS,
          //                         currentPass.pass.state.pipelineLayout, 0,
          //                         1, &desc_set, 0, nullptr);
        }
        last_desc_set = desc_set;

        // Draw
        vkCmdDrawIndexed(
            commandBuffer, pcmd->ElemCount, 1,
            pcmd->IdxOffset + global_idx_offset,
            static_cast<int32_t>(pcmd->VtxOffset + global_vtx_offset), 0);
      }
    }
    global_idx_offset += draw_list->IdxBuffer.Size;
    global_vtx_offset += draw_list->VtxBuffer.Size;
  }
  platform_io.Renderer_RenderState = nullptr;

  // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been
  // called. Our last values will leak into user/application rendering IF:
  // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or
  // VK_DYNAMIC_STATE_SCISSOR dynamic state
  // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself
  // to explicitly set that state. If you use VK_DYNAMIC_STATE_VIEWPORT or
  // VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before
  // rendering. In theory we should aim to backup/restore those values but I am
  // not sure this is possible. We perform a call to vkCmdSetScissor() to set
  // back a full viewport which is likely to fix things for 99% users but
  // technically this is not perfect. (See github #4644)
  VkRect2D scissor = {{0, 0}, {(uint32_t)fb_width, (uint32_t)fb_height}};
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  return Error::Success();
}

auto InitializeImGui(
    Graphics::GraphicsContext &context,
    Graphics::Rendergraph::RenderGraph &graph,
    Graphics::Rendergraph::ResourceHandle lastResourceHandle, // NOLINT
    Graphics::Rendergraph::ResourceHandle writeResourceHandle,
    Context &editorContext) -> Error::Error {
  // Initialize ImGui context

  /// =============================== ///
  /// TODO: Create canvas module again, refactor ImGui code into it
  /// Add support for non compile time external resources in rendergraph
  /// add graphics stack for canvases and integrate the rendergraph in to it as
  /// well, let imgui be it's entirely own thing as it manages it's own
  /// resources
  /// =============================== ///

  std::cout << "Creating shaders for ImGui..." << "\n";

  auto shader = Graphics::Shader::ShaderModule::Create(context, "imgui",
                                                       "ImGui vertex shader");

  if (Error::IsError(shader)) {
    return shader.error();
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
  init_info.DescriptorPool = context.descriptorPools.at(context.frameIndex);
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

  //NOLINTNEXTLINE
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

  bool showDemoWindow = true;

  auto defaultTexture = Graphics::Texture::GetDefaultTexture(
      context, VK_FORMAT_R8G8B8A8_UNORM,
      Graphics::Texture::TextureType::DEFAULT);

  if (Error::IsError(defaultTexture)) {
    return defaultTexture.error();
  }

  auto defaultTextureHandle = Graphics::Rendergraph::ImportTexture(
      graph, defaultTexture.value(),
      {.oldState =
           {
               .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               .stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               .access = VK_ACCESS_SHADER_READ_BIT,
           },
       .newState = {
           .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
           .stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
           .access = VK_ACCESS_SHADER_READ_BIT,
       }});

  // Create render pass for ImGui
  Graphics::Rendergraph::AddRenderPass(
      graph,
      {
          .resources =
              {
                  defaultTextureHandle,
                  writeResourceHandle,
              },
          .viewport = {.x = 0.0F,
                       .y = 0.0F,
                       .width = static_cast<float>(
                           context.swapchainInfo.extent.width),
                       .height = static_cast<float>(
                           context.swapchainInfo.extent.height),
                       .minDepth = -1.0F,
                       .maxDepth = 1.0F},
          .resourceBindings =
              {{
                   .resource = writeResourceHandle,
                   .location = 0,
                   .type = Graphics::Rendergraph::BindingType::Attachment,
                   .usage = Graphics::Rendergraph::ResourceUsage::WriteOnly,
               },
               {
                   .resource = defaultTextureHandle,
                   .binding = 0,
                   .set = 0,
                   .location = 1,
                   .type = Graphics::Rendergraph::BindingType::Sampler,
                   .usage = Graphics::Rendergraph::ResourceUsage::ReadOnly,
               }},
          .shader = shader.value(),
          .executeFunction =
              [&showDemoWindow](
                  VkCommandBuffer cmd, Graphics::GraphicsContext &context,
                  Graphics::Rendergraph::RenderGraph &graph,
                  Graphics::Rendergraph::CompiledPass &currentPass) -> void {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow(&showDemoWindow);

            ImGui::Render();

            auto err = RenderImguiDrawLists(cmd, context, graph, currentPass);

            if (Error::IsError(err)) {
              std::cerr << "Error rendering ImGui draw lists: " << err.message
                        << "\n";
            }
          },
      });

  return Error::Success();
}
} // namespace Editor