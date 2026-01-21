#include "dynamicRendering.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/object.hpp"
#include "slang/slang.h"
#include "tl/expected.hpp"
#include <mutex>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <set>
#ifdef WIN32
#include <cstddef>
#include <stdint.h>
#endif
#include <unordered_map>
#include <utility>

namespace Graphics::RenderTarget {

thread_local std::unordered_map<State, std::pair<VkPipeline, VkPipelineLayout>,
                                StateHash> // NOLINTNEXTLINE Pipeline cache
    PipelineCache = {};

inline auto GetSwapchainRendertarget(const GraphicsContext &context)
    -> Result<Ref<RenderTarget>> {
  static Ref<RenderTarget> swapchainRendertarget = Ref<RenderTarget>::Make();

  auto swapchainTexturesResult = GetSwapchainTextures(context);

  if (Error::IsError(swapchainTexturesResult)) {
    return swapchainTexturesResult.error().AsUnexpected();
  }

  auto texture = swapchainTexturesResult.value()[context.swapchainImageIndex];

  swapchainRendertarget->texture = texture;
  swapchainRendertarget->location = 0;
  swapchainRendertarget->blendMode = DefaultBlendMode;
  swapchainRendertarget->clearValue = {0.0F, 0.0F, 0.0F, 1.0F};
  swapchainRendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  return swapchainRendertarget;
}

inline auto GetRenderExtent(const GraphicsContext &context, const State &state)
    -> VkExtent2D {

  return VkExtent2D{
      .width = state.renderTargets.at(0).get()->texture->size.width,
      .height = state.renderTargets.at(0).get()->texture->size.height,
  };
}

auto GetSwapchainTextures(const GraphicsContext &context)
    -> Result<std::vector<Ref<Graphics::Texture::Texture>>> {
  static std::vector<Ref<Graphics::Texture::Texture>> textures = {};

  if (textures.empty()) {
    textures.reserve(context.swapchainInfo.imageCount);

    for (uint32_t i = 0; i < context.swapchainInfo.imageCount; i++) {
      auto textureResult = Graphics::Texture::FromSwapchainTexture(
          context, context.swapchainInfo.images[i],
          context.swapchainInfo.imageViews[i], context.swapchainInfo.format,
          context.swapchainInfo.extent.width,
          context.swapchainInfo.extent.height);

      if (Error::IsError(textureResult)) {
        return textureResult.error().AsUnexpected();
      }

      textures.emplace_back(textureResult.value());
    }
  }

  return textures;
}

auto GetPipelineLayout(const GraphicsContext &context,
                       const Shader::ShaderModule *shader)
    -> Result<VkPipelineLayout> {
  auto pushConstantRanges = std::vector<VkPushConstantRange>{};

  for (const auto &buffer : shader->pushBuffers) {
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = buffer.GetStageFlags();
    pushConstantRange.offset = buffer.GetBufferOffset();
    pushConstantRange.size = static_cast<uint32_t>(buffer.GetBufferSize());
    if (pushConstantRange.size == 0) {
      return Error::Unexpected(
          "Push buffer has zero size in shader reflection");
    }

    PrintDebug("Adding push constant range: offset {}, size {}",
               pushConstantRange.offset, pushConstantRange.size);

    pushConstantRanges.emplace_back(pushConstantRange);
  }

  PrintDebug("Creating pipeline layout from shader reflection");

  std::vector<VkDescriptorSetLayout> setLayouts{};

  auto maxSet = 0U;
  for (const auto &pair : shader->descriptorSetLayouts) {
    maxSet = (std::max)(maxSet, pair.first);
  }

  PrintDebug("Max descriptor set index: {}", maxSet);

  setLayouts.resize(maxSet + 1);

  for (const auto &pair : shader->descriptorSetLayouts) {
    setLayouts[pair.first] = pair.second;
  }

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

  Error error;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    error = Error::Create(vkCreatePipelineLayout(
        context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
  }

  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  return pipelineLayout;
}

auto FillDescriptorSets(
    GraphicsContext &context, Shader::ShaderModule *shader,
    std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
        &descriptorSetLayoutBindings) -> Error {
  for (auto &layout : shader->reflection.resources) {
    if (layout.IsBuffer()) {
      auto &bufferInfo = std::get<BufferInfo>(layout.info);

      if (bufferInfo.bufferType == BufferType::Uniform ||
          bufferInfo.bufferType == BufferType::Storage) {
        auto layoutBinding = VkDescriptorSetLayoutBinding{
            .binding = bufferInfo.binding,
            .descriptorType = bufferInfo.bufferType == BufferType::Uniform
                                  ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                  : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        };

        descriptorSetLayoutBindings[bufferInfo.set].emplace_back(layoutBinding);
      }
    } else if (layout.IsSampler()) {
      auto &imageInfo = std::get<SamplerInfo>(layout.info);

      auto layoutBinding = VkDescriptorSetLayoutBinding{
          .binding = imageInfo.binding,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_ALL,
          .pImmutableSamplers = nullptr,
      };

      descriptorSetLayoutBindings[imageInfo.set].emplace_back(layoutBinding);
    }
  }

  if (shader->reflection.hasGlobals) {
    auto layoutBinding = VkDescriptorSetLayoutBinding{
        .binding = shader->reflection.globals.binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
        .pImmutableSamplers = nullptr,
    };

    descriptorSetLayoutBindings[shader->reflection.globals.set].emplace_back(
        layoutBinding);
  }

  return Error::Success();
}

auto BindDefaultTextures(GraphicsContext &context, Shader::ShaderModule *shader)
    -> Error {
  for (const auto &resource : shader->reflection.resources) {
    if (resource.IsSampler()) {

      const auto &samplerInfo = std::get<SamplerInfo>(resource.info);

      VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
      Texture::TextureType type = Texture::TextureType::DEFAULT;

      if (samplerInfo.shape == SLANG_TEXTURE_3D) {
        type = Texture::TextureType::VOLUME;
      } else if (samplerInfo.shape == SLANG_TEXTURE_CUBE) {
        type = Texture::TextureType::CUBEMAP;
      } else if (samplerInfo.shape == SLANG_TEXTURE_2D_ARRAY) {
        type = Texture::TextureType::ARRAY;
      }

      auto defaultTextureResult =
          Texture::GetDefaultTexture(context, format, type);

      if (Error::IsError(defaultTextureResult)) {
        return defaultTextureResult.error();
      }

      auto defaultTexture = defaultTextureResult.value();

      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView = defaultTexture->view;
      imageInfo.sampler = defaultTexture->GetSampler(context);
      VkWriteDescriptorSet descriptorWrite{};
      descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet = shader->descriptorSets[samplerInfo.set];

      if (descriptorWrite.dstSet == VK_NULL_HANDLE) {
        return Error::Create(
            "Descriptor set is null when binding default texture.");
      }

      descriptorWrite.dstBinding = samplerInfo.binding;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType =
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pImageInfo = &imageInfo;
      {
        std::lock_guard<std::mutex> lock(
            Graphics::GraphicsContext::mutexes.device);
        vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
      }
    }
  }

  return Error::Success();
}

auto CreateDescriptorSets(GraphicsContext &context,
                          Shader::ShaderModule *shader) -> Error {
  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      descriptorSetLayoutBindings;

  PrintDebug("Creating descriptor sets...");

  auto fillResult =
      FillDescriptorSets(context, shader, descriptorSetLayoutBindings);
  if (Error::IsError(fillResult)) {
    return fillResult;
  }

  shader->descriptorSets.clear();
  shader->descriptorSetLayouts.clear();

  for (const auto &setBinding : descriptorSetLayoutBindings) {
    uint32_t setIndex = setBinding.first;
    const auto &bindings = setBinding.second;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      auto error = Error::Create(vkCreateDescriptorSetLayout(
          context.device, &layoutInfo, nullptr, &descriptorSetLayout));

      if (Error::IsError(error)) {
        return error;
      }
    }

    shader->descriptorSetLayouts[setIndex] = descriptorSetLayout;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.descriptorPools.at(context.frameIndex);
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      auto error = Error::Create(vkAllocateDescriptorSets(
          context.device, &allocInfo, &shader->descriptorSets[setIndex]));
      if (Error::IsError(error)) {
        return error;
      }
    }
  }

  PrintDebug("Buffer descriptor sets created successfully.");

  auto bindResult = BindDefaultTextures(context, shader);
  if (Error::IsError(bindResult)) {
    return bindResult;
  }

  return Error::Success();
}

inline auto GetShaderStages(const State &state)
    -> Result<std::vector<VkPipelineShaderStageCreateInfo>> {

  static std::unordered_map<Shader::ShaderModule *,
                            std::vector<VkPipelineShaderStageCreateInfo>>
      shaderStageCache;

  if (shaderStageCache.contains(state.shader.get())) {
    return shaderStageCache[state.shader.get()];
  }

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};

  auto *shader = state.shader.get();

  if (shader == nullptr) {
    return Error::Unexpected("Shader module is null in GetShaderStages.");
  }

  VkPipelineShaderStageCreateInfo vertexStageInfo = {};
  vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertexStageInfo.module = shader->module;
  vertexStageInfo.pName = "vertexMain";

  shaderStages.emplace_back(vertexStageInfo);

  VkPipelineShaderStageCreateInfo fragmentStageInfo = {};
  fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragmentStageInfo.module = shader->module;
  fragmentStageInfo.pName = "fragmentMain";

  shaderStages.emplace_back(fragmentStageInfo);

  shaderStageCache[state.shader.get()] = shaderStages;

  return shaderStages;
}

auto inline GetInputAssemblyState(const State &state)
    -> VkPipelineInputAssemblyStateCreateInfo {
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = state.primitiveTopology;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  return inputAssembly;
}

auto inline GetRasterizationState(const State &state)
    -> VkPipelineRasterizationStateCreateInfo {
  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = state.polygonMode;
  rasterizer.lineWidth = state.lineWidth;
  rasterizer.cullMode = state.cullMode;
  rasterizer.frontFace = state.frontFace;
  rasterizer.depthBiasEnable = VK_FALSE;

  return rasterizer;
}

auto inline GetColorBlendAttachmentState(const GraphicsContext &context,
                                         const State &state)
    -> Result<std::vector<VkPipelineColorBlendAttachmentState>> {

  // Get Shader Output Reflection //

  auto *shader = state.shader.get();

  auto entryPointIndex =
      shader->entryPointToStageIndex.at(SlangStage::SLANG_STAGE_FRAGMENT);

  auto *entryPoint =
      shader->programLayout->getEntryPointByIndex(entryPointIndex);

  if (entryPoint == nullptr) {
    return Error::Unexpected(
        "Failed to get fragment entry point from shader program layout");
  }

  auto *outputVariableLayout = entryPoint->getResultVarLayout();

  if (outputVariableLayout == nullptr) {
    return Error::Unexpected(
        "Shader has no output variable layout for fragment stage");
  }

  // Determine Expected Output Attachments //

  std::set<uint32_t> expectedAttachments = {};
  for (uint32_t i = 0;
       i < outputVariableLayout->getTypeLayout()->getFieldCount(); ++i) {
    auto *outVar = outputVariableLayout->getTypeLayout()->getFieldByIndex(i);
    if (strcmp(outVar->getSemanticName(), "SV_Target") == 0) {
      expectedAttachments.insert(outVar->getSemanticIndex());
    }
  }

  // Get Actual Set Render Targets //

  auto idx = 0;
  auto blendAttachments = std::vector<VkPipelineColorBlendAttachmentState>(
      state.renderTargets.size() + 1);

  for (const auto &rendertarget : state.renderTargets) {
    int location = rendertarget->location;
    if (location == -1) {
      location = idx;
    }
    idx++;

    blendAttachments.resize(location + 1);
    blendAttachments[location] = rendertarget->blendMode;
  }

  for (uint32_t i = 0; i <= blendAttachments.size(); ++i) {
    if (expectedAttachments.contains(i)) {
      if (i >= blendAttachments.size()) {
        return Error::Unexpected(
            "Missing blend attachment for expected output location " +
            std::to_string(i));
      }
    }
  }

  return blendAttachments;
}

auto inline GetDepthStencilState(const State &state)
    -> VkPipelineDepthStencilStateCreateInfo {
  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = static_cast<VkBool32>(state.depthTestEnable);
  depthStencil.depthWriteEnable = static_cast<VkBool32>(state.depthWriteEnable);
  depthStencil.depthCompareOp = state.depthCompareOp;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable =
      static_cast<VkBool32>(state.stencilTestEnable);

  return depthStencil;
}

auto inline GetRenderFormatInfo(const GraphicsContext &context,
                                const State &state) -> std::vector<VkFormat> {

  auto idx = 0;
  auto formats = std::vector<VkFormat>(state.renderTargets.size() + 1,
                                       VK_FORMAT_UNDEFINED);

  for (const auto &rendertarget : state.renderTargets) {
    int location = rendertarget->location;
    if (location == -1) {
      location = idx;
    }
    idx++;

    formats.resize(location + 1);
    formats[location] = rendertarget->texture->format;
  }

  return formats;
}

inline auto CreateGraphicsPipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, VkPipelineLayout>> {
  if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Unexpected(
        "Attempted to create graphics pipeline with non-graphics bind point.");
  }

  PrintDebug("Creating graphics pipeline");

  auto shaderStagesResult = GetShaderStages(state);
  if (Error::IsError(shaderStagesResult)) {
    return shaderStagesResult.error().AsUnexpected();
  }

  auto shaderStages = shaderStagesResult.value();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};

  auto bindings = state.vertexFormat.GetBindings();
  auto attributes = state.vertexFormat.GetVkAttributes();

  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = bindings.size();
  vertexInputInfo.pVertexBindingDescriptions = bindings.data();
  vertexInputInfo.vertexAttributeDescriptionCount = attributes.size();
  vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

  auto inputAssembly = GetInputAssemblyState(state);
  auto rasterizer = GetRasterizationState(state);

  /// Viewport and Scissor ///

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;

  auto viewport = GetMaximumAllowedViewport();
  auto scissor = GetScissor();

  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  /// Multisampling ///

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  /// Dynamic State ///

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

  std::array<VkDynamicState, 2> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  dynamicState.pDynamicStates = dynamicStates.data();
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());

  /// Depth and Stencil ///

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthTestEnable =
      static_cast<VkBool32>(state.depthTestEnable);
  depthStencilState.depthWriteEnable =
      static_cast<VkBool32>(state.depthWriteEnable);
  depthStencilState.depthCompareOp = state.depthCompareOp;
  depthStencilState.depthBoundsTestEnable = VK_FALSE;
  depthStencilState.stencilTestEnable =
      static_cast<VkBool32>(state.stencilTestEnable);

  /// Attachment Formats ///

  auto idx = 0;
  auto formats = std::vector<VkFormat>(state.renderTargets.size() + 1,
                                       VK_FORMAT_UNDEFINED);

  for (const auto &rendertarget : state.renderTargets) {
    int location = rendertarget->location;
    if (location == -1) {
      location = idx;
    }
    idx++;

    formats.resize(location + 1);
    formats[location] = rendertarget->texture->format;
  }

  /// Color Attachments ///

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = formats.size();
  renderingCreateInfo.pColorAttachmentFormats = formats.data();

  auto colorBlendingResult = GetColorBlendAttachmentState(context, state);

  if (Error::IsError(colorBlendingResult)) {
    return colorBlendingResult.error().AsUnexpected();
  }

  auto blendAttachments = colorBlendingResult.value();

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = blendAttachments.size();
  colorBlending.pAttachments = blendAttachments.data();

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
  pipelineInfo.pDepthStencilState = &depthStencilState;
  pipelineInfo.pDynamicState = &dynamicState;

  auto *shader = state.shader.get();

  auto layoutResult = GetPipelineLayout(context, shader);
  if (Error::IsError(layoutResult)) {
    return layoutResult.error().AsUnexpected();
  }

  pipelineInfo.layout = layoutResult.value();
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &renderingCreateInfo;

  PrintDebug("Creating graphics pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto error = Error::Create(vkCreateGraphicsPipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }
  PipelineCache[state] = {pipeline, layoutResult.value()};

  return std::pair<VkPipeline, VkPipelineLayout>(pipeline,
                                                 layoutResult.value());
}

inline auto CreateComputePipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, VkPipelineLayout>> {
  // Currently not implemented
  // return Error::Unexpected("Compute pipeline creation not implemented.");

  VkComputePipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelineInfo.stage.module = state.shader->module;
  // computeMain doesn't work here because of SPIR-V entry point naming, i guess that only applies to raster stages?
  // See:
  // pCreateInfos[0].stage.pName "computeMain" entry point not found for stage VK_SHADER_STAGE_COMPUTE_BIT.
  // (The only entry point found was "main" for VK_SHADER_STAGE_COMPUTE_BIT
  // Some shading languages will let you name the main function something else, but when compiled to SPIR-V,
  // it will keep it as 'main' to match defaults found in other shading langauges such as GLSL.
  // It is also valid in a single SPIR-V binary to have 'main' for two different stages.
  pipelineInfo.stage.pName = "main";

  auto layoutResult = GetPipelineLayout(context, state.shader.get());
  if (Error::IsError(layoutResult)) {
    return layoutResult.error().AsUnexpected();
  }

  pipelineInfo.layout = layoutResult.value();

  PrintDebug("Creating compute pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto error = Error::Create(vkCreateComputePipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

  return std::pair<VkPipeline, VkPipelineLayout>(pipeline,
                                                 layoutResult.value());
}

inline auto CreatePipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, VkPipelineLayout>> {
  if (state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return CreateGraphicsPipeline(context, state);
  }
  if (state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    return CreateComputePipeline(context, state);
  }

  return Error::Unexpected("Unsupported pipeline bind point.");
}

inline auto GetPipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, VkPipelineLayout>> {

  PrintDebug("Getting pipeline from cache");

  auto cacheIterator = PipelineCache.find(state);

  PrintDebug("Cache iterator found");

  if (cacheIterator != PipelineCache.end()) {
    PrintDebug("Pipeline found in cache");

    return cacheIterator->second;
  }

  auto result = CreatePipeline(context, state);

  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }

  return result.value();
}

// NOLINTNEXTLINE, render target state stack
thread_local static std::vector<State> StateStack{};

// NOLINTNEXTLINE, to keep track of last applied state
thread_local static State LastState{};

inline auto SetupDefaultState(GraphicsContext &context) -> Result<State> {
  auto defaultState = State();

  defaultState.viewport = {};
  defaultState.scissor = {};

  defaultState.shader = Shader::DefaultShaderModule;

  auto targetResult = GetSwapchainRendertarget(context);
  if (Error::IsError(targetResult)) {
    return targetResult.error().AsUnexpected();
  }

  defaultState.renderTargets = {targetResult.value()};

  return defaultState;
}

auto Load(GraphicsContext &context) -> Error {
  assert(StateStack.size() == 0 &&
         "RenderTarget state stack is not empty on Load.");

  auto defaultStateResult = SetupDefaultState(context);
  if (Error::IsError(defaultStateResult)) {
    return defaultStateResult.error();
  }

  StateStack.emplace_back(defaultStateResult.value());

  return Error::Success();
}

auto Push(GraphicsContext &context) -> Error {
  if (StateStack.size() == 0) {
    auto defaultStateResult = SetupDefaultState(context);
    if (Error::IsError(defaultStateResult)) {
      return defaultStateResult.error();
    }

    StateStack.emplace_back(defaultStateResult.value());
  } else {
    StateStack.emplace_back(StateStack.back());
  }

  return Error::Success();
}

auto Pop(GraphicsContext &context) -> Error {
  if (StateStack.size() <= 1 || StateStack.empty()) {
    return Error::Create("More pops than pushes.");
  }

  StateStack.pop_back();

  return Error::Success();
}

auto Reset(GraphicsContext &context) -> Error {
  PrintAlways("Resetting RenderTarget state stack.");

  StateStack.clear();

  auto defaultStateResult = SetupDefaultState(context);
  if (Error::IsError(defaultStateResult)) {
    return defaultStateResult.error();
  }

  StateStack.emplace_back(defaultStateResult.value());

  LastState = State();

  return Error::Success();
}

auto FlushCompute(GraphicsContext &context) -> Result<bool> {
  auto &currentState = StateStack.back();

  if (currentState.bindPoint != VK_PIPELINE_BIND_POINT_COMPUTE) {
    return Error::Unexpected("Current state is not a compute pipeline.");
  }

  auto result = CreateDescriptorSets(context, currentState.shader.get());
  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  auto pipelineResult = GetPipeline(context, currentState);
  if (Error::IsError(pipelineResult)) {
    return pipelineResult.error().AsUnexpected();
  }

  const auto &commandBuffer = Graphics::GetCommandBuffer();

  // Unset current rendering, otherwise vkCmdPipelineBarrier will fail
  // EndRendering(context);

  auto error =
      currentState.shader->FlushBuffers(context, pipelineResult.value().second,
                                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  // Loop over all attachments
  // Swapchain cannot be used as sampler, so we never have to transition it here
  for (const auto &rendertarget : currentState.renderTargets) {
    auto result = rendertarget->texture->UseAsAttachment(context);

    if (Error::IsError(result)) {
      return result.AsUnexpected();
    }
  }

  PrintDebug("Binding pipeline");

  vkCmdBindPipeline(commandBuffer, currentState.bindPoint,
                    pipelineResult.value().first);

  return true;
}

auto FlushGraphics(GraphicsContext &context) -> Result<bool> {
  auto &currentState = StateStack.back();

  if (currentState.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Unexpected("Current state is not a graphics pipeline.");
  }

  auto result = CreateDescriptorSets(context, currentState.shader.get());
  if (Error::IsError(result)) {
    return result.AsUnexpected();
  }

  auto pipelineResult = GetPipeline(context, currentState);
  if (Error::IsError(pipelineResult)) {
    return pipelineResult.error().AsUnexpected();
  }

  const auto &commandBuffer = Graphics::GetCommandBuffer();

  // Unset current rendering, otherwise vkCmdPipelineBarrier will fail
  // EndRendering(context);

  auto viewport = GetClippedViewport();

  auto translationMatrix = Math::Matrix4x4::TranslationMatrix(
      {-viewport.width / 2.0F, -viewport.height / 2.0F, 0.0F}); // NOLINT

  Math::Matrix4x4 projectionMatrix = Math::Matrix4x4::Orthographic(
      viewport.width, viewport.height, 0.0F, 1.0F);

  auto viewProjectionMatrix = projectionMatrix * translationMatrix;

  auto sendErr = currentState.shader->Send(context, {"DefaultProjectionMatrix"},
                                           viewProjectionMatrix.AsByteSpan());
  if (Error::IsError(sendErr)) {
    PrintError("Failed to send projection matrix to shader: {}",
               sendErr.message);
  }

  auto error = currentState.shader->FlushBuffers(
      context, pipelineResult.value().second,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
          VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  // Loop over all attachments
  // Swapchain cannot be used as sampler, so we never have to transition it here
  for (const auto &rendertarget : currentState.renderTargets) {
    auto result = rendertarget->texture->UseAsAttachment(context);

    if (Error::IsError(result)) {
      return result.AsUnexpected();
    }
  }

  PrintDebug("Binding pipeline");

  vkCmdBindPipeline(commandBuffer, currentState.bindPoint,
                    pipelineResult.value().first);

  return true;
}

auto Flush(GraphicsContext &context) -> Result<bool> {
  if (StateStack.back() == LastState && !Graphics::GetIsStateDirty() &&
      GetIsCurrentlyRendering()) {
    return false;
  }

  LastState = StateStack.back();
  Graphics::GetIsStateDirty() = false;

  if (StateStack.back().bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return FlushGraphics(context);
  }
  if (StateStack.back().bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    return FlushCompute(context);
  }

  return Error::Unexpected("Unsupported pipeline bind point in Flush.");
}

auto Destroy(GraphicsContext &context) -> void {
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  for (const auto &pair : PipelineCache) {
    vkDestroyPipeline(context.device, pair.second.first, nullptr);
  }
  PipelineCache.clear();
}

inline auto BeginRendering(GraphicsContext &context) -> Error {
  auto &currentState = StateStack.back();

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

  auto viewport = GetClippedViewport();
  auto scissor = GetScissor();

  renderingInfo.renderArea.offset = {.x = 0, .y = 0};
  renderingInfo.renderArea.extent = GetRenderExtent(context, currentState);
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  if (currentState.renderTargets.size() >
      context.deviceProperties.limits.maxColorAttachments) {
    return Error::Create(
        "Number of bound render targets exceeds device limits.");
  }

  if (renderingInfo.renderArea.extent.width == 0 ||
      renderingInfo.renderArea.extent.height == 0) {
    return Error::Create("Render area has zero width or height.");
  }

  auto colorAttachments = std::vector<VkRenderingAttachmentInfo>{};
  auto depthAttachment = VkRenderingAttachmentInfo{};
  auto stencilAttachment = VkRenderingAttachmentInfo{};

  bool hasDepth = false;
  bool hasStencil = false;

  for (const auto &rendertarget : currentState.renderTargets) {
    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget->texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentInfo.loadOp = rendertarget->loadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget->clearValue;

    if (Image::IsDepthTexture(rendertarget->texture->format)) {
      if (hasDepth) {
        return Error::Create("Multiple depth attachments bound.");
      }
      depthAttachment = attachmentInfo;
      depthAttachment.imageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      hasDepth = true;

      Barrier::UpdateUsage(
          context, *rendertarget->texture,
          {.stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
           .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT});
    } else if (Image::IsStencilTexture(rendertarget->texture->format)) {
      if (hasStencil) {
        return Error::Create("Multiple stencil attachments bound.");
      }
      stencilAttachment = attachmentInfo;
      stencilAttachment.imageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      hasStencil = true;

      Barrier::UpdateUsage(
          context, *rendertarget->texture,
          {.stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
           .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT});
    } else {
      colorAttachments.emplace_back(attachmentInfo);

      // Barrier::UpdateUsage(
      //     context, *rendertarget->texture,
      //     {.stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      //      .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
      //                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT});
    }
  }

  renderingInfo.colorAttachmentCount =
      static_cast<uint32_t>(colorAttachments.size());
  renderingInfo.pColorAttachments = colorAttachments.data();

  if (hasDepth) {
    renderingInfo.pDepthAttachment = &depthAttachment;
  } else {
    renderingInfo.pDepthAttachment = nullptr;
  }

  if (hasStencil) {
    renderingInfo.pStencilAttachment = &stencilAttachment;
  } else {
    renderingInfo.pStencilAttachment = nullptr;
  }

  PrintDebug("Beginning rendering pass");

  vkCmdBeginRendering(Graphics::GetCommandBuffer(), &renderingInfo);

  GetIsCurrentlyRendering() = true;

  for (auto &rendertarget : currentState.renderTargets) {
    // Make sure subsequent renders load from the existing content if we ever need to re-bind mid-pass
    rendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  return Error::Success();
}

auto EndRendering(GraphicsContext &context) -> void {
  if (GetIsCurrentlyRendering()) {
    if (Graphics::GetCommandBuffer() == VK_NULL_HANDLE) {
      PrintWarning(
          "Tried to end rendering, but command buffer is null. Skipping.");
      return;
    }
    vkCmdEndRendering(Graphics::GetCommandBuffer());
    GetIsCurrentlyRendering() = false;
  }
}

auto InsertResourceBarriers(GraphicsContext &context) -> Error {
  auto &currentState = StateStack.back();
  auto &shader = currentState.shader;

  for (auto &bufferPair : shader->boundBuffers) {
    auto &buffer = bufferPair.second;
    auto key = bufferPair.first;

    auto infoResult = shader->GetSlotDescription(key);
    if (Error::IsError(infoResult)) {
      return infoResult.error();
    }

    auto info = infoResult.value().GetInfo<BufferInfo>();

    VkAccessFlags2 access = 0;

    switch (info.access) {
    case SLANG_RESOURCE_ACCESS_READ:
      access = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_READ_WRITE:
      access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_WRITE:
      access = VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_RASTER_ORDERED:
    case SLANG_RESOURCE_ACCESS_APPEND:
    case SLANG_RESOURCE_ACCESS_CONSUME:
    case SLANG_RESOURCE_ACCESS_FEEDBACK:
    case SLANG_RESOURCE_ACCESS_NONE:
    case SLANG_RESOURCE_ACCESS_UNKNOWN:
      break;
    }

    if (access == 0) {
      PrintWarning("Buffer access type is Unknown for slang access: {}, "
                   "skipping barrier.",
                   static_cast<uint32_t>(info.access));
      continue;
    }

    auto stages = VK_PIPELINE_STAGE_2_NONE;

    for (const auto &stage : shader->stages) {
      switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        stages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
        stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        break;
      case VK_SHADER_STAGE_COMPUTE_BIT:
        stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        break;
      default:
        break;
      }
    }

    Barrier::UpdateUsage(context, *buffer,
                         {
                             .stages = stages,
                             .access = access,
                         });
  }

  return Error::Success();
}

auto PrepareRendering(GraphicsContext &context) -> Error {
  auto flushResult = Flush(context);

  if (Error::IsError(flushResult)) {
    return flushResult.error();
  }

  auto updatedState = flushResult.value();
  auto &currentState = StateStack.back();

  auto insertionResult = InsertResourceBarriers(context);

  if (Error::IsError(insertionResult)) {
    return insertionResult;
  }

  if (updatedState) {
    if (currentState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS &&
        !GetIsCurrentlyRendering()) {
      auto beginResult = BeginRendering(context);

      if (Error::IsError(beginResult)) {
        return beginResult;
      }
    }
  }

  if (currentState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    auto viewport = GetClippedViewport();
    auto scissor = GetScissor();

    vkCmdSetViewport(Graphics::GetCommandBuffer(), 0, 1, &viewport);
    vkCmdSetScissor(Graphics::GetCommandBuffer(), 0, 1, &scissor);
  }

  return Error::Success();
}

auto IsSwapchainTexture(const GraphicsContext &context,
                        const Graphics::Texture::Texture &texture)
    -> Result<bool> {
  auto swapchainTexturesResult = GetSwapchainTextures(context);

  if (Error::IsError(swapchainTexturesResult)) {
    return swapchainTexturesResult.error().AsUnexpected();
  }

  const auto &swapchainTextures = swapchainTexturesResult.value();

  for (const auto &swapchainTexture : swapchainTextures) {
    if (swapchainTexture.get() == &texture) {
      return true;
    }
  }

  return false;
}

auto FinalizeFrame(GraphicsContext &context) -> Error {
  if (StateStack.size() != 1) {
    return Error::Create("More pushes than pops.");
  }
  if (StateStack.back().renderTargets.size() != 0) {
    for (const auto &rendertarget : StateStack.back().renderTargets) {
      if (!IsSwapchainTexture(context, *rendertarget->texture)) {
        return Error::Create(
            "Non-swapchain render targets remain bound at end of frame.");
      }
    }
  }
  EndRendering(context);
  return Error::Success();
}

auto BeginFrame(GraphicsContext &context) -> Error {
  // Setup stack with new swapchain texture

  StateStack.clear();

  auto defaultStateResult = SetupDefaultState(context);
  if (Error::IsError(defaultStateResult)) {
    return defaultStateResult.error();
  }

  StateStack.emplace_back(defaultStateResult.value());

  return Error::Success();
}

// Setters //
auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp)
    -> void {
  StateStack.back().depthTestEnable = enable;
  StateStack.back().depthWriteEnable = writeEnable;
  StateStack.back().depthCompareOp = compareOp;
}

auto SetCullMode(VkCullModeFlags cullMode) -> void {
  StateStack.back().cullMode = cullMode;
}

auto SetPolygonMode(VkPolygonMode polygonMode) -> void {
  StateStack.back().polygonMode = polygonMode;
}

auto SetViewport(const VkViewport *viewport) -> void {
  if (viewport == nullptr) {
    StateStack.back().hasViewport = false;
    return;
  }

  StateStack.back().hasViewport = true;
  StateStack.back().viewport = *viewport;
}

auto SetScissor(const VkRect2D *scissor) -> void {
  if (scissor == nullptr) {
    StateStack.back().hasScissor = false;
    return;
  }

  StateStack.back().hasScissor = true;
  StateStack.back().scissor = *scissor;
}

auto ClipScissor(const VkRect2D &scissor) -> void {
  auto &currentScissor = StateStack.back().scissor;

  int32_t rectMinX = std::max(currentScissor.offset.x, scissor.offset.x);
  int32_t rectMinY = std::max(currentScissor.offset.y, scissor.offset.y);
  int32_t rectMaxX =
      std::min(currentScissor.offset.x +
                   static_cast<int32_t>(currentScissor.extent.width),
               scissor.offset.x + static_cast<int32_t>(scissor.extent.width));
  int32_t rectMaxY =
      std::min(currentScissor.offset.y +
                   static_cast<int32_t>(currentScissor.extent.height),
               scissor.offset.y + static_cast<int32_t>(scissor.extent.height));

  if (rectMaxX < rectMinX || rectMaxY < rectMinY) {
    // No intersection, set to zero area
    currentScissor.offset.x = 0;
    currentScissor.offset.y = 0;
    currentScissor.extent.width = 0;
    currentScissor.extent.height = 0;
  } else {
    currentScissor.offset.x = rectMinX;
    currentScissor.offset.y = rectMinY;
    currentScissor.extent.width = static_cast<uint32_t>(rectMaxX - rectMinX);
    currentScissor.extent.height = static_cast<uint32_t>(rectMaxY - rectMinY);
  }
}

auto SetShader(const Ref<Shader::ShaderModule> &shader) -> void {
  if (shader.get() == nullptr) {
    StateStack.back().shader = Shader::DefaultShaderModule;
  } else {
    StateStack.back().shader = shader;
  }
}

auto SetRenderTargets(const std::vector<Ref<RenderTarget>> &renderTargets)
    -> Error {

  if (renderTargets.empty()) {
    return Error::Create("No render targets provided.");
  }

  StateStack.back().renderTargets = renderTargets;
  SetViewport(nullptr);

  return Error::Success();
}

auto SetLineWidth(float lineWidth) -> void {
  StateStack.back().lineWidth = lineWidth;
}

auto SetWindingOrder(VkFrontFace frontFace) -> void {
  StateStack.back().frontFace = frontFace;
}

auto SetVertexFormat(const VertexFormat &vertexFormat) -> void {
  StateStack.back().vertexFormat = vertexFormat;
}

auto SetTopology(VkPrimitiveTopology topology) -> void {
  StateStack.back().primitiveTopology = topology;
}

// Getters //

auto GetDepthMode() -> std::tuple<bool, bool, VkCompareOp> {
  auto &currentState = StateStack.back();
  return {currentState.depthTestEnable, currentState.depthWriteEnable,
          currentState.depthCompareOp};
}

auto GetCullMode() -> VkCullModeFlags {
  auto &currentState = StateStack.back();
  return currentState.cullMode;
}

auto GetPolygonMode() -> VkPolygonMode {
  auto &currentState = StateStack.back();
  return currentState.polygonMode;
}

auto GetMaximumAllowedViewport() -> VkViewport {
  auto &currentState = StateStack.back();
  auto viewport = currentState.viewport;

  auto size = currentState.renderTargets[0]->texture->size;

  viewport.width = static_cast<float>(size.width);
  viewport.height = static_cast<float>(size.height);

  viewport.width = (std::max)(viewport.width, 1.0F);
  viewport.height = (std::max)(viewport.height, 1.0F);

  return viewport;
}

auto GetClippedViewport() -> VkViewport {
  auto &currentState = StateStack.back();

  if (!currentState.hasViewport) {
    return GetMaximumAllowedViewport();
  }

  auto viewport = currentState.viewport;

  // Default to size of current attachments
  auto size = currentState.renderTargets[0]->texture->size;

  viewport.width = std::min(viewport.width, static_cast<float>(size.width));
  viewport.height = std::min(viewport.height, static_cast<float>(size.height));

  return viewport;
}

auto GetViewport() -> VkViewport {
  auto &currentState = StateStack.back();

  if (!currentState.hasViewport) {
    return GetMaximumAllowedViewport();
  }

  // return currentState.viewport;
  return {
      .x = currentState.viewport.x,
      .y = currentState.viewport.y,
      .width = (std::max)(currentState.viewport.width, 1.0F),
      .height = (std::max)(currentState.viewport.height, 1.0F),
      .minDepth = currentState.viewport.minDepth,
      .maxDepth = currentState.viewport.maxDepth,
  };
}

auto GetScissor() -> VkRect2D {
  auto &currentState = StateStack.back();

  if (!currentState.hasScissor) {
    VkRect2D scissor = {};
    auto viewport = GetMaximumAllowedViewport();
    scissor.offset = {.x = 0, .y = 0};
    scissor.extent = {
        .width = static_cast<uint32_t>(viewport.width),
        .height = static_cast<uint32_t>(viewport.height),
    };

    scissor.extent.width = (std::max)(scissor.extent.width, 1U);
    scissor.extent.height = (std::max)(scissor.extent.height, 1U);

    return scissor;
  }

  // return currentState.scissor;

  return {
      .offset =
          {
              .x = currentState.scissor.offset.x,
              .y = currentState.scissor.offset.y,
          },
      .extent =
          {
              .width = (std::max)(currentState.scissor.extent.width, 1U),
              .height = (std::max)(currentState.scissor.extent.height, 1U),
          },
  };
}

auto GetShader() -> Ref<Shader::ShaderModule> {
  auto &currentState = StateStack.back();
  if (currentState.shader.get() == Shader::DefaultShaderModule.get()) {
    return Ref<Shader::ShaderModule>(nullptr);
  }
  return currentState.shader;
}

auto GetRenderTargets() -> std::vector<Ref<RenderTarget>> {
  auto &currentState = StateStack.back();
  return currentState.renderTargets;
}

auto GetLineWidth() -> float {
  auto &currentState = StateStack.back();
  return currentState.lineWidth;
}

auto GetWindingOrder() -> VkFrontFace {
  auto &currentState = StateStack.back();
  return currentState.frontFace;
}

auto GetVertexFormat() -> VertexFormat {
  auto &currentState = StateStack.back();
  return currentState.vertexFormat;
}

auto GetTopology() -> VkPrimitiveTopology {
  auto &currentState = StateStack.back();
  return currentState.primitiveTopology;
}

auto SetBindPoint(VkPipelineBindPoint bindPoint) -> void {
  StateStack.back().bindPoint = bindPoint;
}
auto GetBindPoint() -> VkPipelineBindPoint {
  auto &currentState = StateStack.back();
  return currentState.bindPoint;
}

auto Clear(GraphicsContext &context, const ClearInfo &clearInfo) -> Error {
  auto &currentState = StateStack.back();

  auto *commandBuffer = Graphics::GetCommandBuffer();

  auto count = currentState.renderTargets.size();

  if (count == 0) {
    return Error::Create("No render targets to clear.");
  }

  std::vector<VkClearAttachment> clearAttachments{};
  std::vector<VkClearRect> clearRects{};

  VkClearRect clearRect = {};
  auto viewport = GetClippedViewport();

  clearRect.rect.offset = {
      .x = static_cast<int32_t>(viewport.x),
      .y = static_cast<int32_t>(viewport.y),
  };
  clearRect.rect.extent = {
      .width = static_cast<uint32_t>(viewport.width),
      .height = static_cast<uint32_t>(viewport.height),
  };
  clearRect.baseArrayLayer = 0;
  clearRect.layerCount = 1;

  for (uint32_t i = 0; i < count; ++i) {
    VkClearAttachment clearAttachment = {};
    auto &rendertarget = currentState.renderTargets[i];

    if (Image::IsDepthTexture(rendertarget->texture->format) ||
        Image::IsStencilTexture(rendertarget->texture->format)) {
      clearAttachment.aspectMask = 0;
      bool doClear = false;
      if (Image::IsDepthTexture(rendertarget->texture->format)) {
        clearAttachment.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        doClear = clearInfo.clearDepth;
      }
      if (Image::IsStencilTexture(rendertarget->texture->format)) {
        clearAttachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        doClear = clearInfo.clearStencil;
      }
      clearAttachment.clearValue.depthStencil.depth = clearInfo.depthClearValue;
      clearAttachment.clearValue.depthStencil.stencil =
          clearInfo.stencilClearValue;
      if (doClear) {
        clearAttachments.emplace_back(clearAttachment);
        clearRects.emplace_back(clearRect);
      }
    } else {
      clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      clearAttachment.colorAttachment = i;
      auto color = clearInfo.colors.size() > i ? clearInfo.colors[i]
                                               : Color{0.0F, 0.0F, 0.0F, 1.0F};
      clearAttachment.clearValue.color.float32[0] = color.r;
      clearAttachment.clearValue.color.float32[1] = color.g;
      clearAttachment.clearValue.color.float32[2] = color.b;
      clearAttachment.clearValue.color.float32[3] = color.a;

      clearAttachments.emplace_back(clearAttachment);
      clearRects.emplace_back(clearRect);
    }
  }

  // TODO: Cache this step and only flush on state changes
  auto error = PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdClearAttachments(
      commandBuffer, static_cast<uint32_t>(clearAttachments.size()),
      clearAttachments.data(), static_cast<uint32_t>(clearRects.size()),
      clearRects.data());

  return Error::Success();
}

} // namespace Graphics::RenderTarget
