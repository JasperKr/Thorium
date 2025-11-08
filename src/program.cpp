#include "program.hpp"
#include "Graphics/canvas.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include "Modules/timer.hpp"
#include <iostream>

namespace Program {
struct Vertex {
  float position[3]; // NOLINT
  float color[3];    // NOLINT
};

static inline auto GetMeshes() -> std::vector<Graphics::Mesh<Vertex>> & {
  static std::vector<Graphics::Mesh<Vertex>> meshes = {};
  return meshes;
}

static inline auto GetShaders()
    -> std::vector<Graphics::Shader::ShaderModule> & {
  static std::vector<Graphics::Shader::ShaderModule> shaders = {};
  return shaders;
}

static inline auto GetPipeline() -> VkPipeline & {
  static VkPipeline pipeline = VK_NULL_HANDLE;
  return pipeline;
}

auto Configuration(ApplicationConfig &config) -> Error::Error {
  config.Title = "Thorium Engine - Program Example";

  return Error::Success();
}

auto Load(Graphics::GraphicsContext &context) -> Error::Error {
  auto fsResult = Graphics::Shader::ShaderModule::Create(
      context, "src/Graphics/Shaders/default.fs", VK_SHADER_STAGE_FRAGMENT_BIT,
      "Default fragment shader");
  if (Error::IsError(fsResult)) {
    return fsResult.error();
  }

  auto vsResult = Graphics::Shader::ShaderModule::Create(
      context, "src/Graphics/Shaders/default.vs", VK_SHADER_STAGE_VERTEX_BIT,
      "Default vertex shader");

  if (Error::IsError(vsResult)) {
    return vsResult.error();
  }

  auto fragmentShader = fsResult.value();
  auto vertexShader = vsResult.value();

  GetShaders().push_back(vertexShader);
  GetShaders().push_back(fragmentShader);

  Graphics::VertexFormat format = {};
  format.Attributes = {
      {.location = 0,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = offsetof(Vertex, position)},
      {.location = 1,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = offsetof(Vertex, color)},
  };
  format.Bindings = {
      {.binding = 0,
       .stride = sizeof(Vertex),
       .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
  };

  VkVertexInputBindingDescription bindingDescription = format.Bindings[0];

  uint32_t vertexCount = 4;
  std::vector<Vertex> vertexData = {

      {.position = {-0.5F, -0.5F, 0.0F}, .color = {1, 0, 0}}, // NOLINT
      {.position = {0.5F, -0.5F, 0.0F}, .color = {0, 1, 0}},  // NOLINT
      {.position = {0.5F, 0.5F, 0.0F}, .color = {0, 0, 1}},   // NOLINT
      {.position = {-0.5F, 0.5F, 0.0F}, .color = {1, 1, 1}}}; // NOLINT

  std::vector<uint32_t> indexData = {0, 1, 2, 2, 3, 0};

  std::cout << "Creating mesh..." << "\n";

  auto meshResult =
      Graphics::Mesh<Vertex>::Create(context, format, vertexData, &indexData);

  if (Error::IsError(meshResult)) {
    std::cerr << "Failed to create mesh: " << meshResult.error().message
              << "\n";
    return meshResult.error();
  }

  auto mesh = meshResult.value();

  GetMeshes().push_back(mesh);

  std::cout << "Mesh created successfully." << "\n";

  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = context.swapchainInfo.format;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  // Create pipeline
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  VkPipelineLayout pipelineLayout = nullptr;
  vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr,
                         &pipelineLayout);

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkViewport viewport = {};
  viewport.x = 0.0F;
  viewport.y = 0.0F;
  viewport.width = (float)context.swapchainInfo.extent.width;
  viewport.height = (float)context.swapchainInfo.extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  VkRect2D scissor = {};
  scissor.offset = VkOffset2D{0, 0};
  scissor.extent = context.swapchainInfo.extent;

  viewportState.pViewports = &viewport;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0F;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask =
      static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT);

  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 0;
  dynamicState.pDynamicStates = nullptr;

  std::cout << "Creating pipeline layout..." << "\n";

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = mesh.Format.Bindings.size();
  vertexInputInfo.pVertexBindingDescriptions = mesh.Format.Bindings.data();
  vertexInputInfo.vertexAttributeDescriptionCount =
      mesh.Format.Attributes.size();
  vertexInputInfo.pVertexAttributeDescriptions = mesh.Format.Attributes.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  std::cout << "Creating shader stages..." << "\n";

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {};
  shaderStages.push_back({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertexShader.module,
      .pName = "main",
  });

  shaderStages.push_back({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragmentShader.module,
      .pName = "main",
  });

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = 1;
  std::vector<VkFormat> colorFormats = {context.swapchainInfo.format};
  renderingCreateInfo.pColorAttachmentFormats = colorFormats.data();

  // Create graphics pipeline
  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.pRasterizationState = &rasterizer;
  pipelineCreateInfo.pMultisampleState = &multisampling;
  pipelineCreateInfo.pColorBlendState = &colorBlending;
  pipelineCreateInfo.pDepthStencilState = &depthStencil;
  pipelineCreateInfo.pDynamicState = &dynamicState;
  pipelineCreateInfo.layout = pipelineLayout;
  pipelineCreateInfo.renderPass = VK_NULL_HANDLE; // to be set later
  pipelineCreateInfo.subpass = 0;
  pipelineCreateInfo.pStages = shaderStages.data();
  pipelineCreateInfo.pNext = &renderingCreateInfo;

  std::cout << "Creating graphics pipeline..." << "\n";

  return Error::FromVkResult(
      vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
                                &pipelineCreateInfo, nullptr, &GetPipeline()));
}

auto Update(double deltaTime) -> Error::Error {
  std::cout << "Delta time: " << deltaTime << "s; " << Timer::GetFPS()
            << " FPS\n";

  return Error::Success();
}

auto Draw(Graphics::GraphicsContext &context) -> Error::Error {
  Graphics::SetCanvas(context, {}, nullptr);
  vkCmdBindPipeline(Graphics::GetCommandBuffer(context, 0),
                    VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline());
  for (auto &mesh : GetMeshes()) {
    mesh.Draw(context);
  }

  return Error::Success();
}

auto Exit(Graphics::GraphicsContext &context) -> Error::Error {
  vkDestroyPipeline(context.device, GetPipeline(), nullptr);
  for (auto &shader : GetShaders()) {
    vkDestroyShaderModule(context.device, shader.module, nullptr);
  }
  for (auto &mesh : GetMeshes()) {
    mesh.Destroy(context);
  }

  return Error::Success();
}
} // namespace Program