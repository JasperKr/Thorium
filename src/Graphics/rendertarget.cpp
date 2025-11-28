#include "rendertarget.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"
#include "tl/expected.hpp"
#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstdint>
#include <set>
#include <unordered_map>

namespace Graphics::RenderTarget {

auto GetPipelineLayout(const GraphicsContext &context, const State &state)
    -> VkPipelineLayout {
  auto shader = Shader::GetShaderModule(state.shader);
  auto *globalLayout = shader.programLayout->getGlobalParamsVarLayout();

  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      setBindingsMap;

  auto *typeLayout = globalLayout->getTypeLayout();

  for (uint32_t i = 0; i < typeLayout->getFieldCount(); ++i) {
    auto *fieldLayout = typeLayout->getFieldByIndex(i);
    auto bindingRange = fieldLayout->getBindingRange();

    if (bindingRange.setIndex >= UINT32_MAX) {
      continue;
    }

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = bindingRange.bindingIndex;
    binding.descriptorCount = 1;

    switch (fieldLayout->getCategory()) {
    case slang::ParameterCategory::ConstantBuffer:
      binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      break;
    case slang::ParameterCategory::MutableResource:
    case slang::ParameterCategory::ImmutableResource:
      switch (fieldLayout->getResourceType()) {
      case slang::ResourceType::Texture_SRV:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;
      case slang::ResourceType::SamplerState:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        break;
      case slang::ResourceType::StructuredBuffer_SRV:
      case slang::ResourceType::ByteAddressBuffer_SRV:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        break;
      default:
        assert(false && "Unsupported resource type in pipeline layout");
      }
      break;
    default:
      assert(false && "Unsupported parameter category in pipeline layout");
    }

    binding.stageFlags = VK_SHADER_STAGE_ALL;

    setBindingsMap[bindingRange.setIndex].emplace_back(binding);
  }
}

inline auto CreatePipeline(const GraphicsContext &context, const State &state)
    -> tl::expected<VkPipeline, Error::Error> {
  if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return tl::make_unexpected(
        Error::Create("Only graphics pipelines are supported currently"));
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

  auto &shader = Shader::GetShaderModule(state.shader);

  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = shader.module;
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = shader.module;
  shaderStages[1].pName = "main";

  auto vertexformat =
      Graphics::PredefinedVertexFormats.at(shader.GetExpectedVertexFormat());

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = vertexformat.Bindings.size();
  vertexInputInfo.pVertexBindingDescriptions = vertexformat.Bindings.data();
  vertexInputInfo.vertexAttributeDescriptionCount =
      vertexformat.Attributes.size();
  vertexInputInfo.pVertexAttributeDescriptions = vertexformat.Attributes.data();
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &state.viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &state.scissor;

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

  auto entryPointIndex =
      shader.entryPointToStageIndex.at(SlangStage::SLANG_STAGE_FRAGMENT);
  auto *entryPoint =
      shader.programLayout->getEntryPointByIndex(entryPointIndex);
  auto *outputVariableLayout = entryPoint->getResultVarLayout();

  std::set<uint32_t> expectedAttachments = {};
  for (uint32_t i = 0;
       i < outputVariableLayout->getTypeLayout()->getFieldCount(); ++i) {
    auto *outVar = outputVariableLayout->getTypeLayout()->getFieldByIndex(i);
    if (strcmp(outVar->getSemanticName(), "SV_Target") == 0) {
      expectedAttachments.insert(outVar->getSemanticIndex());
    }
  }

  uint32_t attachmentCount = 0;
  auto idx = 0;
  auto blendAttachments = std::vector<VkPipelineColorBlendAttachmentState>(
      state.renderTargets.size() + 1);
  auto formats = std::vector<VkFormat>(state.renderTargets.size() + 1,
                                       VK_FORMAT_UNDEFINED);

  bool hasDepthAttachment = false;
  bool hasStencilAttachment = false;

  // Loop over attachments and use GetPassAttachmentInfo to fetch blend modes
  for (const auto &rendertarget : state.renderTargets) {
    int location = rendertarget.location;
    if (location == -1) {
      location = idx;
    }
    idx++;

    blendAttachments.resize(location + 1);
    formats.resize(location + 1);

    attachmentCount++;
    blendAttachments[location] = rendertarget.blendMode;
    formats[location] = rendertarget.texture.format;

    if (Texture::IsDepthTexture(rendertarget.texture.format)) {
      hasDepthAttachment = true;
    } else if (Texture::IsStencilTexture(rendertarget.texture.format)) {
      hasStencilAttachment = true;
    }
  }

  for (uint32_t i = 0; i <= blendAttachments.size(); ++i) {
    if (expectedAttachments.contains(i)) {
      if (i >= blendAttachments.size()) {
        return tl::make_unexpected(Error::Create(
            "Missing blend attachment for expected output location " +
            std::to_string(i)));
      }
    }
  }

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = attachmentCount;
  colorBlending.pAttachments = blendAttachments.data();

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

  renderingCreateInfo.colorAttachmentCount = attachmentCount;
  std::cout << "Creating graphics pipeline with " << attachmentCount
            << " color attachments."
            << "\n";

  renderingCreateInfo.pColorAttachmentFormats = formats.data();

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 0;
  dynamicState.pDynamicStates = nullptr;

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = GetPipelineLayout(context, state);
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &renderingCreateInfo;

  VkPipeline pipeline = VK_NULL_HANDLE;

  auto error = Error::Create(vkCreateGraphicsPipelines(
      context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

  PipelineCache[state] = pipeline;

  if (Error::IsError(error)) {
    return tl::make_unexpected(error);
  }

  return pipeline;
}

inline auto GetPipeline(const GraphicsContext &context, const State &state)
    -> tl::expected<VkPipeline, Error::Error> {
  auto cacheIterator = PipelineCache.find(state);

  if (cacheIterator != PipelineCache.end()) {
    return cacheIterator->second;
  }

  auto result = CreatePipeline(context, state);

  if (Error::IsError(result)) {
    return result;
  }

  return result.value();
}
} // namespace Graphics::RenderTarget