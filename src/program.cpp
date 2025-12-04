#include "program.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/rendergraph.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include "Modules/timer.hpp"
#include <cassert>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <cstddef>
#include <iostream>

#include "Modules/Editor/imgui.hpp"

namespace Program {
struct Vertex {
  float position[3]; // NOLINT
  float color[3];    // NOLINT
};

static inline auto GetMeshes() -> std::vector<Ref<Graphics::Mesh>> & {
  static std::vector<Ref<Graphics::Mesh>> meshes = {};
  return meshes;
}

static inline auto GetShaders()
    -> std::vector<Ref<Graphics::Shader::ShaderModule>> & {
  static std::vector<Ref<Graphics::Shader::ShaderModule>> shaders = {};
  return shaders;
}

static inline auto GetPipeline() -> VkPipeline & {
  static VkPipeline pipeline = VK_NULL_HANDLE;
  return pipeline;
}

static inline auto GetRenderGraph() -> Graphics::Rendergraph::RenderGraph & {
  static Graphics::Rendergraph::RenderGraph graph = {};
  return graph;
}

static inline auto GetSwapchainHandleIndex() -> size_t & {
  static size_t index = 0;
  return index;
}

static inline auto GetSwapchainTextures()
    -> std::vector<Ref<Graphics::Texture::Texture>> & {
  static std::vector<Ref<Graphics::Texture::Texture>> textures = {};
  return textures;
}

auto Load(Graphics::GraphicsContext &context) -> Error::Error {

  // Setup render graph to debug it

  auto &graph = GetRenderGraph();
  std::vector<Graphics::Rendergraph::ResourceHandle> textureHandles;
  textureHandles.reserve(static_cast<size_t>(4 * 4));
  for (int i = 0; i < 4 * 4; i++) {
    textureHandles.emplace_back(Graphics::Rendergraph::AddTexture(
        graph,
        {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .width = context.swapchainInfo.extent.width,
            .height = context.swapchainInfo.extent.height,
            .mipLevels = 1,
            .lifetime = Graphics::Rendergraph::ResourceLifetime::Transient,
            .usage =
                static_cast<uint32_t>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT),

        }));
  }

  auto shaderResult = Graphics::Shader::ShaderModule::Create(context, "default",
                                                             "Default shader");
  if (Error::IsError(shaderResult)) {
    return shaderResult.error();
  }

  GetShaders().emplace_back(shaderResult.value());

  // Create swapchain textures

  auto &swapchainTextures = GetSwapchainTextures();
  swapchainTextures.reserve(context.swapchainInfo.imageCount);

  for (uint32_t i = 0; i < context.swapchainInfo.imageCount; i++) {
    auto textureResult = Graphics::Texture::FromSwapchainTexture(
        context, context.swapchainInfo.images[i], context.swapchainInfo.format,
        context.swapchainInfo.extent.width,
        context.swapchainInfo.extent.height);

    if (Error::IsError(textureResult)) {
      return textureResult.error();
    }

    swapchainTextures.emplace_back(textureResult.value());
  }

  // Import swapchain texture
  // We will swap the texture pointer during rendering
  auto swapchainHandle = Graphics::Rendergraph::ImportTexture(
      graph, swapchainTextures[0],
      {
          .oldState =
              {
                  .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              },
          .newState =
              {
                  .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              },
      });

  GetSwapchainHandleIndex() = swapchainHandle;

  std::cout << "Swapchain extent: " << context.swapchainInfo.extent.width << "x"
            << context.swapchainInfo.extent.height << "\n";

  Editor::Context imguiContext = {};

  // auto imguiErr = Editor::InitializeImGui(context, graph,
  // textureHandles.at(0), swapchainHandle, imguiContext);

  // if (Error::IsError(imguiErr)) {
  // std::cerr << "Failed to initialize ImGui: " << imguiErr.message << "\n";
  // return imguiErr;
  // }

  auto graphErr = Graphics::Rendergraph::Compile(context, graph);

  if (Error::IsError(graphErr)) {
    std::cout << "Failed to compile render graph: " << graphErr.message << "\n";
    return graphErr;
  }

  uint32_t vertexCount = 4;
  std::vector<Vertex> vertexData = {

      {.position = {-0.5F, -0.5F, 0.0F}, .color = {1, 0, 0}}, // NOLINT
      {.position = {0.5F, -0.5F, 0.0F}, .color = {0, 1, 0}},  // NOLINT
      {.position = {0.5F, 0.5F, 0.0F}, .color = {0, 0, 1}},   // NOLINT
      {.position = {-0.5F, 0.5F, 0.0F}, .color = {1, 1, 1}}}; // NOLINT

  std::vector<uint32_t> indexData = {0, 1, 2, 2, 3, 0};

  std::cout << "Creating mesh..." << "\n";

  auto meshResult =
      Graphics::Mesh::Create(context, format, vertexData, &indexData);

  if (Error::IsError(meshResult)) {
    std::cerr << "Failed to create mesh: " << meshResult.error().message
              << "\n";
    return meshResult.error();
  }

  auto mesh = meshResult.value();

  GetMeshes().emplace_back(mesh);

  return Error::Success();
}

auto Update(double deltaTime) -> Error::Error {
  std::cout << "Delta time: " << deltaTime << "s; " << Timer::GetFPS()
            << " FPS\n";

  return Error::Success();
}

auto Draw(Graphics::GraphicsContext &context) -> Error::Error {
  auto handle = GetSwapchainHandleIndex();
  auto swapchainIndex = context.swapchainImageIndex;
  auto &swapchainTextures = GetSwapchainTextures();

  auto texture = swapchainTextures[swapchainIndex];
  auto &graph = GetRenderGraph();

  std::cout << "Updating swapchain texture handle to image index "
            << swapchainIndex << "\n";

  graph.resources[handle].info = texture;

  Graphics::Rendergraph::Execute(context, graph,
                                 Graphics::GetCommandBuffer(context, 0));

  return Error::Success();
}

auto Exit(Graphics::GraphicsContext &context) -> Error::Error {
  vkDestroyPipeline(context.device, GetPipeline(), nullptr);
  for (auto &shader : GetShaders()) {
    auto &module = *shader.get();
    vkDestroyShaderModule(context.device, module.module, nullptr);
  }
  for (auto &mesh : GetMeshes()) {
    mesh->Destroy(context);
  }

  return Error::Success();
}
} // namespace Program