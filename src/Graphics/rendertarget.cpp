#include "rendertarget.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/object.hpp"
#include "slang/slang.h"
#include "tl/expected.hpp"
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

auto GetSwapchainTextures() -> std::vector<Ref<Graphics::Texture::Texture>> & {
  static std::vector<Ref<Graphics::Texture::Texture>> textures = {};
  return textures;
}

auto GetPipelineLayout(const GraphicsContext &context,
                       const Shader::ShaderModule *shader)
    -> tl::expected<VkPipelineLayout, Error::Error> {
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
  auto error = Error::Create(vkCreatePipelineLayout(
      context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

  if (Error::IsError(error)) {
    return tl::make_unexpected(error);
  }

  return pipelineLayout;
}

inline auto GetSwapchainRendertarget(const GraphicsContext &context)
    -> Ref<RenderTarget> {
  static Ref<RenderTarget> swapchainRendertarget = Ref<RenderTarget>::Make();
  auto texture = GetSwapchainTextures()[context.swapchainImageIndex];

  swapchainRendertarget->texture = texture;
  swapchainRendertarget->location = 0;
  swapchainRendertarget->blendMode = DefaultBlendMode;
  swapchainRendertarget->clearValue = {0.0F, 0.0F, 0.0F, 1.0F};
  return swapchainRendertarget;
}

auto CreateDescriptorSets(GraphicsContext &context,
                          Shader::ShaderModule *shader) -> Error::Error {
  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      descriptorSetLayoutBindings;

  PrintDebug("Creating descriptor sets...");

  for (auto &layout : shader->reflection.resources) {
    if (layout.variant == ResourceVariant::Buffer) {
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
    } else if (layout.variant == ResourceVariant::Sampler) {
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
    auto error = Error::Create(vkCreateDescriptorSetLayout(
        context.device, &layoutInfo, nullptr, &descriptorSetLayout));

    shader->descriptorSetLayouts[setIndex] = descriptorSetLayout;

    if (Error::IsError(error)) {
      return error;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.descriptorPools.at(context.frameIndex);
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    error = Error::Create(vkAllocateDescriptorSets(
        context.device, &allocInfo, &shader->descriptorSets[setIndex]));
    if (Error::IsError(error)) {
      return error;
    }
  }

  PrintDebug("Buffer descriptor sets created successfully.");

  // loop over all samplers and bind the default texture
  for (const auto &resource : shader->reflection.resources) {
    if (resource.variant == ResourceVariant::Sampler) {

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
      descriptorWrite.dstBinding = samplerInfo.binding;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType =
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pImageInfo = &imageInfo;
      vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
    } else if (resource.variant == ResourceVariant::Buffer) {
      const auto &bufferInfo = std::get<BufferInfo>(resource.info);
      if (bufferInfo.bufferType == BufferType::Uniform ||
          bufferInfo.bufferType == BufferType::Storage) {
        VkDescriptorBufferInfo bufferInfoDesc{};
        bufferInfoDesc.offset = 0;
        bufferInfoDesc.range = VK_WHOLE_SIZE;
        // Buffer would be set during actual execution
        bufferInfoDesc.buffer = VK_NULL_HANDLE;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = shader->descriptorSets[bufferInfo.set];
        descriptorWrite.dstBinding = bufferInfo.binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType =
            bufferInfo.bufferType == BufferType::Uniform
                ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfoDesc;

        vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
      }
    }
  }

  return Error::Success();
}

inline auto CreatePipeline(const GraphicsContext &context, const State &state)
    -> tl::expected<std::pair<VkPipeline, VkPipelineLayout>, Error::Error> {
  if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Unexpected("Only graphics pipelines are supported currently");
  }

  PrintDebug("Creating graphics pipeline");

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

  auto *shader = state.shader.get();

  if (shader == nullptr) {
    return Error::Unexpected("Shader module is null in CreatePipeline.");
  }

  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = shader->module;
  shaderStages[0].pName = "vertexMain";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = shader->module;
  shaderStages[1].pName = "fragmentMain";

  PrintDebug("Setting up vertex input state");

  auto vertexformat =
      Graphics::PredefinedVertexFormats.at(shader->GetExpectedVertexFormat());

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = vertexformat.Bindings.size();
  vertexInputInfo.pVertexBindingDescriptions = vertexformat.Bindings.data();
  vertexInputInfo.vertexAttributeDescriptionCount =
      vertexformat.Attributes.size();
  vertexInputInfo.pVertexAttributeDescriptions = vertexformat.Attributes.data();
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

  PrintDebug("Setting up input assembly state");

  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

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

  PrintDebug("Determining expected output attachments");

  auto entryPointIndex =
      shader->entryPointToStageIndex.at(SlangStage::SLANG_STAGE_FRAGMENT);
  PrintDebug("Fragment entry point index: {}", entryPointIndex);

  PrintDebug("programLayout: {}",
             static_cast<const void *>(shader->programLayout));
  PrintDebug("EntryPointByIndex(1): {}",
             static_cast<const void *>(
                 shader->programLayout->getEntryPointByIndex(1)));
  PrintDebug("EntryPointByIndex(1) name: {}",
             shader->programLayout->getEntryPointByIndex(1)->getName());

  auto *entryPoint =
      shader->programLayout->getEntryPointByIndex(entryPointIndex);

  if (entryPoint == nullptr) {
    return Error::Unexpected(
        "Failed to get fragment entry point from shader program layout");
  }

  PrintDebug("Fetched entry point reflection.");
  auto *outputVariableLayout = entryPoint->getResultVarLayout();

  if (outputVariableLayout == nullptr) {
    return Error::Unexpected(
        "Shader has no output variable layout for fragment stage");
  }

  PrintDebug("Determining expected output attachments");

  std::set<uint32_t> expectedAttachments = {};
  for (uint32_t i = 0;
       i < outputVariableLayout->getTypeLayout()->getFieldCount(); ++i) {
    auto *outVar = outputVariableLayout->getTypeLayout()->getFieldByIndex(i);
    if (strcmp(outVar->getSemanticName(), "SV_Target") == 0) {
      expectedAttachments.insert(outVar->getSemanticIndex());
    }
  }

  std::vector<Ref<RenderTarget>> renderTargets = state.renderTargets;

  if (state.renderTargets.empty() ||
      state.renderTargets[0]->texture.get() == nullptr) {
    renderTargets.clear();
    renderTargets.push_back(GetSwapchainRendertarget(context));

    if (!state.renderTargets.empty()) {
      renderTargets.back()->location = state.renderTargets[0]->location;
      renderTargets.back()->blendMode = state.renderTargets[0]->blendMode;
      renderTargets.back()->clearValue = state.renderTargets[0]->clearValue;
    }
  }

  uint32_t attachmentCount = 0;
  auto idx = 0;
  auto blendAttachments = std::vector<VkPipelineColorBlendAttachmentState>(
      renderTargets.size() + 1);
  auto formats =
      std::vector<VkFormat>(renderTargets.size() + 1, VK_FORMAT_UNDEFINED);

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;

  auto viewport = GetMaximumAllowedViewport();

  auto scissor = GetScissor();

  if (scissor.extent.width == 0 || scissor.extent.height == 0) {
    scissor.extent.width = static_cast<uint32_t>(viewport.width);
    scissor.extent.height = static_cast<uint32_t>(viewport.height);
  }

  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  bool hasDepthAttachment = false;
  bool hasStencilAttachment = false;

  // Loop over attachments and use GetPassAttachmentInfo to fetch blend modes
  for (const auto &rendertarget : renderTargets) {
    int location = rendertarget->location;
    if (location == -1) {
      location = idx;
    }
    idx++;

    blendAttachments.resize(location + 1);
    formats.resize(location + 1);

    attachmentCount++;
    blendAttachments[location] = rendertarget->blendMode;
    formats[location] = rendertarget->texture->format;

    if (Image::IsDepthTexture(rendertarget->texture->format)) {
      hasDepthAttachment = true;
    } else if (Image::IsStencilTexture(rendertarget->texture->format)) {
      hasStencilAttachment = true;
    }
  }

  PrintDebug("Expected attachments:");
  for (const auto &att : expectedAttachments) {
    PrintDebug(" - {}", att);
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

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = attachmentCount;
  colorBlending.pAttachments = blendAttachments.data();

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

  renderingCreateInfo.colorAttachmentCount = attachmentCount;
  PrintDebug("Creating graphics pipeline with {} color attachments.",
             attachmentCount);

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

  PrintDebug("Getting pipeline layout");

  auto layoutResult = GetPipelineLayout(context, shader);
  if (Error::IsError(layoutResult)) {
    return tl::make_unexpected(layoutResult.error());
  }

  pipelineInfo.layout = layoutResult.value();
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &renderingCreateInfo;

  PrintDebug("Creating graphics pipeline...");
  PrintDebug("device: {}", static_cast<const void *>(context.device));

  VkPipeline pipeline = VK_NULL_HANDLE;
  auto error = Error::Create(vkCreateGraphicsPipelines(
      context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

  PrintDebug("storing pipeline in cache");
  PipelineCache[state] = {pipeline, layoutResult.value()};

  if (Error::IsError(error)) {
    return tl::make_unexpected(error);
  }

  return std::pair<VkPipeline, VkPipelineLayout>(pipeline,
                                                 layoutResult.value());
}

inline auto GetPipeline(const GraphicsContext &context, const State &state)
    -> tl::expected<std::pair<VkPipeline, VkPipelineLayout>, Error::Error> {

  PrintDebug("Getting pipeline from cache");

  auto cacheIterator = PipelineCache.find(state);

  PrintDebug("Cache iterator found");

  if (cacheIterator != PipelineCache.end()) {
    PrintDebug("Pipeline found in cache");

    return cacheIterator->second;
  }

  auto result = CreatePipeline(context, state);

  if (Error::IsError(result)) {
    return tl::make_unexpected(result.error());
  }

  return result.value();
}

// NOLINTNEXTLINE, render target state stack
static std::vector<State> StateStack{};

// NOLINTNEXTLINE, to keep track of last applied state
static State LastState{};

// NOLINTNEXTLINE, for rendergraph and present, which do not use the stack but manually modify the vk state
static bool Dirty;

auto SetDirty() -> void { Dirty = true; }

inline auto SetupDefaultState(GraphicsContext &context) -> State {
  auto defaultState = State();

  defaultState.viewport = {
      .x = 0.0F,
      .y = 0.0F,
      .width = 0.0F,
      .height = 0.0F,
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };

  defaultState.scissor = {
      .offset = {0, 0},
      .extent = {0, 0},
  };

  defaultState.shader = Shader::DefaultShaderModule;

  return defaultState;
}

auto Load(GraphicsContext &context) -> Error::Error {
  assert(StateStack.size() == 0 &&
         "RenderTarget state stack is not empty on Load.");

  StateStack.emplace_back(SetupDefaultState(context));

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

  return Error::Success();
}

auto Push(GraphicsContext &context) -> void {
  if (StateStack.size() == 0) {
    StateStack.emplace_back(SetupDefaultState(context));
  } else {
    StateStack.emplace_back(StateStack.back());
  }
}

auto Pop(GraphicsContext &context) -> Error::Error {
  if (StateStack.size() <= 1) {
    return Error::Create("More pops than pushes.");
  }

  StateStack.pop_back();

  return Error::Success();
}

auto Reset(GraphicsContext &context) -> void {
  StateStack.clear();
  StateStack.emplace_back(SetupDefaultState(context));

  LastState = State();
}

auto Flush(GraphicsContext &context) -> tl::expected<bool, Error::Error> {

  auto &currentState = StateStack.back();
  if (currentState == LastState && !Dirty) {
    return false; // No state change
  }

  LastState = currentState;

  auto result = CreateDescriptorSets(context, currentState.shader.get());
  if (Error::IsError(result)) {
    return tl::make_unexpected(result);
  }

  PrintDebug("Flushing render target state changes");
  auto pipelineResult = GetPipeline(context, currentState);
  if (Error::IsError(pipelineResult)) {
    return tl::make_unexpected(pipelineResult.error());
  }

  const auto &commandBuffer =
      Graphics::GetCommandBuffer(context, GetCurrentThreadIndex());

  PrintDebug("Flushing shader buffers");

  // Unset current rendering, otherwise vkCmdPipelineBarrier will fail
  EndRendering(context);

  auto error =
      currentState.shader->FlushBuffers(context, pipelineResult.value().second);
  if (Error::IsError(error)) {
    return tl::make_unexpected(error);
  }

  PrintDebug("Binding pipeline");

  vkCmdBindPipeline(commandBuffer, currentState.bindPoint,
                    pipelineResult.value().first);

  return true;
}

auto Destroy(GraphicsContext &context) -> void {
  for (const auto &pair : PipelineCache) {
    vkDestroyPipeline(context.device, pair.second.first, nullptr);
  }
  PipelineCache.clear();
}

// NOLINTNEXTLINE, to call vkCmdEndRendering
static bool BegunRendering = false;

inline auto BeginRendering(GraphicsContext &context) -> void {
  auto &currentState = StateStack.back();

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

  auto viewport = GetClippedViewport();

  auto translationMatrix = Math::Matrix4x4::TranslationMatrix(
      {-viewport.width / 2.0F, -viewport.height / 2.0F, 0.0F});

  Math::Matrix4x4 projectionMatrix = Math::Matrix4x4::Orthographic(
      viewport.width, viewport.height, 0.0F, 1.0F);

  auto viewProjectionMatrix = projectionMatrix * translationMatrix;

  auto sendErr = currentState.shader->Send(context, "DefaultProjectionMatrix",
                                           viewProjectionMatrix.AsSpan());
  if (Error::IsError(sendErr)) {
    PrintError("Failed to send projection matrix to shader: {}",
               sendErr.message);
  }

  renderingInfo.renderArea.offset = {.x = static_cast<int32_t>(viewport.x),
                                     .y = static_cast<int32_t>(viewport.y)};
  renderingInfo.renderArea.extent = {
      .width = static_cast<uint32_t>(viewport.width),
      .height = static_cast<uint32_t>(viewport.height)};
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  auto colorAttachments = std::vector<VkRenderingAttachmentInfo>{};
  auto depthAttachment = VkRenderingAttachmentInfo{};
  auto stencilAttachment = VkRenderingAttachmentInfo{};

  std::vector<Ref<RenderTarget>> renderTargets = currentState.renderTargets;

  if (currentState.renderTargets.empty() ||
      currentState.renderTargets[0]->texture.get() == nullptr) {
    renderTargets.clear();
    renderTargets.push_back(GetSwapchainRendertarget(context));

    if (!currentState.renderTargets.empty()) {
      renderTargets.back()->location = currentState.renderTargets[0]->location;
      renderTargets.back()->blendMode =
          currentState.renderTargets[0]->blendMode;
      renderTargets.back()->clearValue =
          currentState.renderTargets[0]->clearValue;
    }
  }

  for (const auto &rendertarget : renderTargets) {
    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget->texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget->clearValue;

    if (Image::IsDepthTexture(rendertarget->texture->format)) {
      depthAttachment = attachmentInfo;
      depthAttachment.imageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else if (Image::IsStencilTexture(rendertarget->texture->format)) {
      stencilAttachment = attachmentInfo;
      stencilAttachment.imageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else {
      colorAttachments.push_back(attachmentInfo);
    }
  }

  renderingInfo.colorAttachmentCount =
      static_cast<uint32_t>(colorAttachments.size());
  renderingInfo.pColorAttachments = colorAttachments.data();
  renderingInfo.pDepthAttachment = nullptr;
  renderingInfo.pStencilAttachment = nullptr;

  vkCmdBeginRendering(
      Graphics::GetCommandBuffer(context, GetCurrentThreadIndex()),
      &renderingInfo);

  BegunRendering = true;
}

auto EndRendering(GraphicsContext &context) -> void {
  if (BegunRendering) {
    vkCmdEndRendering(
        Graphics::GetCommandBuffer(context, GetCurrentThreadIndex()));
    BegunRendering = false;
  }
}

auto PrepareDraw(GraphicsContext &context) -> Error::Error {
  auto flushResult = Flush(context);

  if (Error::IsError(flushResult)) {
    return flushResult.error();
  }

  auto updatedState = flushResult.value();

  if (updatedState) {
    PrintDebug("Beginning rendering");
    BeginRendering(context);
  }

  return Error::Success();
}

auto FinalizeFrame(GraphicsContext &context) -> Error::Error {
  if (StateStack.size() != 1) {
    return Error::Create("More pushes than pops.");
  }
  if (StateStack.back().renderTargets.size() != 0) {
    return Error::Create("Render targets not cleared at end of frame.");
  }
  EndRendering(context);
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

auto SetViewport(const VkViewport &viewport) -> void {
  StateStack.back().viewport = viewport;
}

auto SetScissor(const VkRect2D &scissor) -> void {
  StateStack.back().scissor = scissor;
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
    -> void {
  StateStack.back().renderTargets = renderTargets;
  SetViewport({
      // Signal default size
      .x = 0.0F,
      .y = 0.0F,
      .width = 0.0F,
      .height = 0.0F,
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  });
}

auto SetLineWidth(float lineWidth) -> void {
  StateStack.back().lineWidth = lineWidth;
}

auto SetWindingOrder(VkFrontFace frontFace) -> void {
  StateStack.back().frontFace = frontFace;
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

auto GetViewport() -> VkViewport {
  auto &currentState = StateStack.back();

  return currentState.viewport;
}

auto GetClippedViewport() -> VkViewport {
  auto &currentState = StateStack.back();
  auto viewport = currentState.viewport;

  // Default to size of current attachments
  auto renderTargets = currentState.renderTargets;
  if (renderTargets.empty() || renderTargets[0]->texture.get() == nullptr) {
    renderTargets.emplace_back(
        GetSwapchainRendertarget(*Graphics::GetCurrentGraphicsContext()));
  }

  auto size = renderTargets[0]->texture->size;

  viewport.width = std::min(viewport.width, static_cast<float>(size.width));
  viewport.height = std::min(viewport.height, static_cast<float>(size.height));

  if (viewport.width == 0.0F || viewport.height == 0.0F) {
    viewport.width = static_cast<float>(size.width);
    viewport.height = static_cast<float>(size.height);
  }

  return viewport;
}

auto GetMaximumAllowedViewport() -> VkViewport {
  auto &currentState = StateStack.back();
  auto viewport = currentState.viewport;

  // Default to size of current attachments
  auto renderTargets = currentState.renderTargets;
  if (renderTargets.empty() || renderTargets[0]->texture.get() == nullptr) {
    renderTargets.emplace_back(
        GetSwapchainRendertarget(*Graphics::GetCurrentGraphicsContext()));
  }

  auto size = renderTargets[0]->texture->size;

  viewport.width = static_cast<float>(size.width);
  viewport.height = static_cast<float>(size.height);

  return viewport;
}

auto GetScissor() -> VkRect2D {
  auto &currentState = StateStack.back();
  return currentState.scissor;
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

} // namespace Graphics::RenderTarget
