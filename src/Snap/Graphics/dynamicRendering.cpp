#include "dynamicRendering.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/snapshot.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/object.hpp"
#include "slang/slang.h"
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <variant>
#include <vector>

#include "vulkan/vulkan_core.h"
#include <array>
#include <cassert>
#include <cstdint>
#ifdef WIN32
#include <cstddef>
#include <stdint.h>
#endif
#include <unordered_map>
#include <utility>

#include "../external/tracy/public/tracy/Tracy.hpp"

namespace Graphics::DynamicRendering {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

constexpr size_t PipelineCacheSize = 512UL;

LRUCache<State, std::pair<VkPipeline, PipelineLayout>, StateHash>
    PipelineCache(PipelineCacheSize);
thread_local std::vector<Ref<Shader::ShaderModule>> UsedShaderModules{};
thread_local PipelineLayout CurrentPipelineLayout; // NOLINT

// Only used for cleanup
std::mutex PipelinesMutex;
std::vector<VkPipeline> Pipelines;

// Only used for cleanup
std::mutex PipelineLayoutsMutex;
std::vector<PipelineLayout> PipelineLayouts;

thread_local std::vector<State> StateStack{};

thread_local State LastStateStorage;
thread_local State *LastState = nullptr;

thread_local State *TopOfStack = nullptr;

thread_local bool StateUpdated = false;

std::mutex DescriptorSetLayoutCacheMutex;
std::unordered_map<DescriptorSetLayoutKey, VkDescriptorSetLayout,
                   DescriptorSetLayoutKeyHash>
    DescriptorSetLayoutCache;

thread_local std::unordered_map<DescriptorKey, VkDescriptorSet,
                                DescriptorKeyHash>
    DescriptorSetCache{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto GetRenderExtent(const GraphicsContext &context, const State &state)
    -> VkExtent2D {

  return VkExtent2D{
      .width = state.renderTargets.at(0).texture->size.width,
      .height = state.renderTargets.at(0).texture->size.height,
  };
}

auto GetDescriptorSetLayout(const DescriptorSetLayoutKey &layoutKey,
                            const GraphicsContext &context)
    -> Result<VkDescriptorSetLayout> {
  ZoneScoped;

  const auto &bindings = layoutKey.bindings;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

  std::lock_guard<std::mutex> lock(DescriptorSetLayoutCacheMutex);

  if (DescriptorSetLayoutCache.contains(layoutKey)) {
    descriptorSetLayout = DescriptorSetLayoutCache.at(layoutKey);
  } else {
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    CHECK_ERR(Error::Create(vkCreateDescriptorSetLayout(
        context.device, &layoutInfo, GetAllocationCallbacks(),
        &descriptorSetLayout)));

    DescriptorSetLayoutCache[layoutKey] = descriptorSetLayout;
  }

  return descriptorSetLayout;
}

auto TryCreateShaderDescriptorBindingInfo(const GraphicsContext &context,
                                          Shader::ShaderModule *shader)
    -> Error {
  ZoneScoped;

  // No need to fill if already done
  if (!shader->bindingInfos.empty()) {
    return Error::Success();
  }

  for (auto &layout : shader->reflection.resources) {
    if (layout.IsBuffer()) {
      auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

      if (bufferInfo.bufferType == Reflect::BufferType::Uniform ||
          bufferInfo.bufferType == Reflect::BufferType::Storage) {
        auto layoutBinding = VkDescriptorSetLayoutBinding{
            .binding = bufferInfo.binding,
            .descriptorType =
                bufferInfo.bufferType == Reflect::BufferType::Uniform
                    ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                    : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        };

        shader->bindingInfos[bufferInfo.set].emplace_back(layoutBinding);
      }
    } else if (layout.IsSampler()) {
      auto &imageInfo = std::get<Reflect::SamplerInfo>(layout.info);

      auto layoutBinding = VkDescriptorSetLayoutBinding{
          .binding = imageInfo.binding,
          .descriptorType = imageInfo.access == SLANG_RESOURCE_ACCESS_READ
                                ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_ALL,
          .pImmutableSamplers = nullptr,
      };

      shader->bindingInfos[imageInfo.set].emplace_back(layoutBinding);
    }
  }

  if (shader->reflection.hasGlobals) {
    auto layoutBinding = VkDescriptorSetLayoutBinding{
        .binding = shader->reflection.globals.binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
        .pImmutableSamplers = nullptr,
    };

    shader->bindingInfos[shader->reflection.globals.set].emplace_back(
        layoutBinding);
  }

  for (auto &setPair : shader->bindingInfos) {
    std::ranges::sort(setPair.second,
                      [](const VkDescriptorSetLayoutBinding &first,
                         const VkDescriptorSetLayoutBinding &second) -> bool {
                        return first.binding < second.binding;
                      });
  }

  return Error::Success();
}

inline auto DescriptorTypeToString(VkDescriptorType type) -> std::string_view {
  switch (type) {
  case VK_DESCRIPTOR_TYPE_SAMPLER:
    return "SAMPLER";
  case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    return "COMBINED_IMAGE_SAMPLER";
  case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    return "SAMPLED_IMAGE";
  case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    return "STORAGE_IMAGE";
  case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    return "UNIFORM_TEXEL_BUFFER";
  case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    return "STORAGE_TEXEL_BUFFER";
  case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    return "UNIFORM_BUFFER";
  case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    return "STORAGE_BUFFER";
  case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    return "UNIFORM_BUFFER_DYNAMIC";
  case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    return "STORAGE_BUFFER_DYNAMIC";
  case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    return "INPUT_ATTACHMENT";
  case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
    return "INLINE_UNIFORM_BLOCK";
  case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
    return "ACCELERATION_STRUCTURE_KHR";
  case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
    return "ACCELERATION_STRUCTURE_NV";
  case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
    return "SAMPLE_WEIGHT_IMAGE_QCOM";
  case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
    return "BLOCK_MATCH_IMAGE_QCOM";
  case VK_DESCRIPTOR_TYPE_TENSOR_ARM:
    return "TENSOR_ARM";
  case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
    return "MUTABLE_EXT";
  case VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV:
    return "PARTITIONED_ACCELERATION_STRUCTURE_NV";
  case VK_DESCRIPTOR_TYPE_MAX_ENUM:
    return "MAX_ENUM";
    break;
  }
}

auto State::ToString() const -> std::string {
  std::string result;

  result = std::format("Shader: {}\n", shader ? shader->moduleName : "null");

  for (size_t i = 0; i < renderTargets.size(); ++i) {
    const auto &target = renderTargets[i];
    result = std::format("{}Render Target {}: {}\n", result, i,
                         target.texture->GetDebugName());
  }

  result = std::format("{}Depth Test Enable: {}\n", result,
                       depthTestEnable ? "true" : "false");
  result = std::format("{}Depth Write Enable: {}\n", result,
                       depthWriteEnable ? "true" : "false");
  result = std::format("{}Depth Compare Op: {}\n", result, (int)depthCompareOp);
  result = std::format("{}Stencil Test Enable: {}\n", result,
                       stencilTestEnable ? "true" : "false");
  result = std::format("{}Polygon Mode: {}\n", result, (int)polygonMode);
  result =
      std::format("{}Bind Point: {}", result, static_cast<uint32_t>(bindPoint));

  return result;
}

auto GetPipelineLayout(const GraphicsContext &context,
                       Shader::ShaderModule *shader) -> Result<PipelineLayout> {
  ZoneScoped;
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

  CHECK_ERR(TryCreateShaderDescriptorBindingInfo(context, shader));

  // Find the maximum set index from the shader reflection
  uint32_t maxSet = 0;
  for (const auto &setPair : shader->bindingInfos) {
    maxSet = std::max<unsigned long>(setPair.first, maxSet);
  }

  setLayouts.resize(maxSet + 1);

  // For each set, build the DescriptorSetLayoutKey and get the layout
  for (const auto &setPair : shader->bindingInfos) {
    DescriptorSetLayoutKey layoutKey;
    layoutKey.flags = 0;
    layoutKey.bindings = setPair.second;
    layoutKey.bindingFlags = {};

    setLayouts[setPair.first] =
        CHECK_RES(GetDescriptorSetLayout(layoutKey, context));
  }

  PrintDebug("Max descriptor set index: {}", maxSet);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_ERR(Error::Create(
        vkCreatePipelineLayout(context.device, &pipelineLayoutInfo,
                               GetAllocationCallbacks(), &pipelineLayout)));
  }

  {
    std::lock_guard<std::mutex> lock(PipelineLayoutsMutex);
    PipelineLayouts.emplace_back(pipelineLayout);
  }

  return PipelineLayout{
      .layout = pipelineLayout,
      .descriptorSetLayouts = std::move(setLayouts),
  };
}

auto BindDefaultTextures(const GraphicsContext &context,
                         Shader::ShaderModule *shader) -> Error {
  ZoneScoped;
  auto &state = shader->GetState();

  for (const auto &resource : shader->reflection.resources) {
    if (resource.IsSampler()) {
      const auto &samplerInfo = std::get<Reflect::SamplerInfo>(resource.info);
      auto key = Utils::SetBindingToSlot(samplerInfo.set, samplerInfo.binding);
      if (state.userBoundTextures.contains(key)) {
        continue;
      }

      VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
      TextureType type = TextureType::DEFAULT;

      if (samplerInfo.shape == SLANG_TEXTURE_3D) {
        type = TextureType::VOLUME;
      } else if (samplerInfo.shape == SLANG_TEXTURE_CUBE) {
        type = TextureType::CUBEMAP;
      } else if (samplerInfo.shape == SLANG_TEXTURE_2D_ARRAY) {
        type = TextureType::ARRAY;
      }

      auto defaultTextureResult = GetDefaultTexture(context, format, type);

      if (Error::IsError(defaultTextureResult)) {
        return defaultTextureResult.error();
      }

      auto defaultTexture = defaultTextureResult.value();

      state.userBoundTextures[key] = {defaultTexture, &samplerInfo};
    }
  }

  return Error::Success();
}

inline auto GetShaderStages(const State &state)
    -> Result<std::vector<VkPipelineShaderStageCreateInfo>> {
  ZoneScoped;

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
  rasterizer.lineWidth = 1.0F;
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

  std::unordered_set<uint32_t> expectedAttachments = {};
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
    int location = rendertarget.location;
    if (location == -1) {
      location = idx;
    }
    idx++;

    if (rendertarget.texture->IsDepthTexture() ||
        rendertarget.texture->IsStencilTexture()) {
      continue;
    }

    blendAttachments.resize(location + 1);
    blendAttachments[location] = rendertarget.blendMode;
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

auto inline GetRenderFormatInfo(const GraphicsContext &context,
                                const State &state) -> std::vector<VkFormat> {

  auto idx = 0;
  auto formats = std::vector<VkFormat>(state.renderTargets.size() + 1,
                                       VK_FORMAT_UNDEFINED);

  for (const auto &rendertarget : state.renderTargets) {
    int location = rendertarget.location;
    if (location == -1) {
      location = idx;
    }
    idx++;

    formats.resize(location + 1);
    formats[location] = rendertarget.texture->format;
  }

  return formats;
}

inline auto CreateGraphicsPipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, PipelineLayout>> {
  ZoneScoped;
  if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Unexpected(
        "Attempted to create graphics pipeline with non-graphics bind point.");
  }

  PrintDebug("Creating graphics pipeline");

  auto shaderStages = CHECK_RES(GetShaderStages(state));
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

  std::array<VkDynamicState, 3> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
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
  VkFormat depthFormat = VK_FORMAT_UNDEFINED;
  VkFormat stencilFormat = VK_FORMAT_UNDEFINED;

  for (const auto &rendertarget : state.renderTargets) {

    if (rendertarget.texture->IsDepthTexture()) {
      depthFormat = rendertarget.texture->format;
    } else if (rendertarget.texture->IsStencilTexture()) {
      stencilFormat = rendertarget.texture->format;
    } else {
      int location = rendertarget.location;
      if (location == -1) {
        location = idx;
      }
      idx++;

      formats.resize(location + 1);

      formats[location] = rendertarget.texture->format;
    }
  }

  /// Color Attachments ///

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = formats.size();
  renderingCreateInfo.pColorAttachmentFormats = formats.data();
  renderingCreateInfo.depthAttachmentFormat = depthFormat;
  renderingCreateInfo.stencilAttachmentFormat = stencilFormat;

  auto colorBlendingResult = GetColorBlendAttachmentState(context, state);

  if (Error::IsError(colorBlendingResult)) {
    return colorBlendingResult.error();
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
  pipelineInfo.pVertexInputState = nullptr; // Dynamic vertex input state
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &depthStencilState;
  pipelineInfo.pDynamicState = &dynamicState;

  auto *shader = state.shader.get();
  auto layout = CHECK_RES(GetPipelineLayout(context, shader));

  pipelineInfo.layout = layout.layout;
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &renderingCreateInfo;

  PrintDebug("Creating graphics pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_ERR(Error::Create(vkCreateGraphicsPipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
        GetAllocationCallbacks(), &pipeline)));
  }
  PipelineCache.emplace(state, std::make_pair(pipeline, layout));

  {
    std::lock_guard<std::mutex> lock(PipelinesMutex);
    Pipelines.emplace_back(pipeline);
  }

  return std::pair<VkPipeline, PipelineLayout>(pipeline, layout);
}

inline auto CreateComputePipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, PipelineLayout>> {
  ZoneScoped;

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

  auto layout = CHECK_RES(GetPipelineLayout(context, state.shader.get()));
  pipelineInfo.layout = layout.layout;

  PrintDebug("Creating compute pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_ERR(Error::Create(vkCreateComputePipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
        GetAllocationCallbacks(), &pipeline)));
  }

  PipelineCache.emplace(state, std::make_pair(pipeline, layout));

  {
    std::lock_guard<std::mutex> lock(PipelinesMutex);
    Pipelines.emplace_back(pipeline);
  }

  return std::pair<VkPipeline, PipelineLayout>(pipeline, layout);
}

inline auto CreatePipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, PipelineLayout>> {
  ZoneScoped;
  if (state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return CreateGraphicsPipeline(context, state);
  }
  if (state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    return CreateComputePipeline(context, state);
  }

  return Error::Unexpected("Unsupported pipeline bind point.");
}

inline auto GetPipeline(const GraphicsContext &context, State &state)
    -> Result<std::pair<VkPipeline, PipelineLayout>> {
  ZoneScoped;

  auto *pipeline = PipelineCache.get(state);

  if (pipeline != nullptr) {
    return *pipeline;
  }

  auto result = CreatePipeline(context, state);

  if (Error::IsError(result)) {
    return result.error();
  }

  return result.value();
}

// All commands queued to swapchain must happen while the swapchain is bound
// Thus if this is true and a command is used at frame count != queued frame count
// we have an error
// NOLINTNEXTLINE
thread_local bool DrawnToSwapchain = false;

inline auto SetupDefaultState(const GraphicsContext &context) -> Result<State> {
  auto defaultState = State();

  defaultState.viewport = {};
  defaultState.scissor = {};

  defaultState.shader = Shader::DefaultShaderModule;

  auto const &texture =
      context.swapchainInfo.textures[context.swapchainImageIndex];

  RenderTarget swapchainRendertarget{};

  swapchainRendertarget.texture = texture;
  swapchainRendertarget.location = 0;
  swapchainRendertarget.blendMode = DefaultBlendMode;
  swapchainRendertarget.clearValue = {0.0F, 0.0F, 0.0F, 1.0F};
  swapchainRendertarget.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  PrintDebug("Setup default state with swapchain handle: {}",
             (void *)texture->view);

  defaultState.renderTargets = {swapchainRendertarget};

  return defaultState;
}

auto Load(const GraphicsContext &context) -> Error {
  assert(StateStack.size() == 0 &&
         "RenderTarget state stack is not empty on Load.");

  auto state = CHECK_RES(SetupDefaultState(context));

  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();

  return Error::Success();
}

auto Push(const GraphicsContext &context) -> Error {
  if (StateStack.size() == 0) {
    auto state = CHECK_RES(SetupDefaultState(context));
    StateStack.emplace_back(state);
  } else {
    StateStack.emplace_back(*TopOfStack);
  }
  TopOfStack = &StateStack.back();

  return Error::Success();
}

auto Pop(const GraphicsContext &context) -> Error {
  if (StateStack.size() <= 1 || StateStack.empty()) {
    return Error::Create("More pops than pushes.");
  }

  StateStack.pop_back();
  TopOfStack = &StateStack.back();

  return Error::Success();
}

auto Reset(const GraphicsContext &context) -> Error {
  StateStack.clear();

  auto state = CHECK_RES(SetupDefaultState(context));

  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();
  LastState = nullptr;

  return Error::Success();
}

auto Shutdown(const GraphicsContext &context) -> Error {
  StateStack.clear();
  UsedShaderModules.clear();
  LastState = nullptr;
  TopOfStack = nullptr;
  PipelineCache.clear();

  return Error::Success();
}

auto FlushCompute(const GraphicsContext &context) -> Result<bool> {
  ZoneScoped;

  if (TopOfStack->bindPoint != VK_PIPELINE_BIND_POINT_COMPUTE) {
    return Error::Unexpected("Current state is not a compute pipeline.");
  }

  auto pipeline = CHECK_RES(GetPipeline(context, *TopOfStack));

  auto *commandBuffer = Graphics::GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Unexpected("Command buffer is null in FlushCompute.");
  }

  assert(TopOfStack->shader->stages.at(0) == VK_SHADER_STAGE_COMPUTE_BIT);

  UsedShaderModules.emplace_back(TopOfStack->shader);

  CurrentPipelineLayout = pipeline.second;

  PrintDebug("Binding pipeline");

  vkCmdBindPipeline(commandBuffer, TopOfStack->bindPoint, pipeline.first);

  return true;
}

auto IsSwapchainTexture(const GraphicsContext &context,
                        const Graphics::Texture &texture) -> bool {
  for (const auto &swapchainTexture : context.swapchainInfo.textures) {
    if (swapchainTexture.get() == &texture) {
      return true;
    }
  }

  return false;
}

auto FlushGraphics(const GraphicsContext &context) -> Result<bool> {
  ZoneScoped;

  if (TopOfStack->bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Unexpected("Current state is not a graphics pipeline.");
  }

  for (const auto &rendertarget : TopOfStack->renderTargets) {
    if (IsSwapchainTexture(context, *rendertarget.texture)) {
      DrawnToSwapchain = true;
      break;
    }
  }

  auto pipeline = CHECK_RES(GetPipeline(context, *TopOfStack));

  auto *commandBuffer = Graphics::GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Unexpected("Command buffer is null in FlushGraphics.");
  }

  auto viewport = GetClippedViewport();

  auto translationMatrix = Math::Matrix4x4::TranslationMatrix(
      {-viewport.width / 2.0F, -viewport.height / 2.0F, 0.0F}); // NOLINT

  Math::Matrix4x4 projectionMatrix = Math::Matrix4x4::Orthographic(
      viewport.width, viewport.height, 0.0F, 1.0F);

  auto viewProjectionMatrix = translationMatrix * projectionMatrix;

  static auto projectionMatrixKey = ResourceKey{"DefaultProjectionMatrix"};

  if (TopOfStack->shader->GetUniform(projectionMatrixKey) != nullptr) {
    auto sendErr = Shader::UniformWriter::Send(
        TopOfStack->shader, context, projectionMatrixKey, viewProjectionMatrix);

    if (Error::IsError(sendErr)) {
      PrintError("Failed to send projection matrix to shader: {}",
                 sendErr.message);
    }
  }

  CurrentPipelineLayout = pipeline.second;

  PrintDebug("Binding pipeline");

  {
    ZoneScopedN("vkCmdBindPipeline");
    vkCmdBindPipeline(commandBuffer, TopOfStack->bindPoint, pipeline.first);
  }

  return true;
}

auto Flush(const GraphicsContext &context) -> Result<bool> {
  ZoneScoped;

  if (!Graphics::GetIsStateDirty() && LastState != nullptr &&
      TopOfStack->GetHash() == LastState->GetHash() &&
      *TopOfStack == *LastState) {
    return false;
  }

  LastStateStorage = *TopOfStack; // Copy current state to last state storage
  LastState = &LastStateStorage;  // Point last state to the storage

  if (TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    auto result = FlushGraphics(context);
    return result;
  }
  if (TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    auto result = FlushCompute(context);
    return result;
  }

  Graphics::GetIsStateDirty() = false;
  return Error::Unexpected("Unsupported pipeline bind point in Flush.");
}

auto Destroy(const GraphicsContext &context) -> void {
  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device, PipelinesMutex);
  for (const auto &pipeline : Pipelines) {
    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(context.device, pipeline, GetAllocationCallbacks());
    }
  }
  for (const auto &layout : PipelineLayouts) {
    vkDestroyPipelineLayout(context.device, layout.layout,
                            GetAllocationCallbacks());
  }
  for (auto &layouts : DynamicRendering::DescriptorSetLayoutCache) {
    vkDestroyDescriptorSetLayout(context.device, layouts.second,
                                 GetAllocationCallbacks());
  }

  Pipelines.clear();
  PipelineLayouts.clear();
  DynamicRendering::DescriptorSetLayoutCache.clear();
}

inline auto BeginRendering(const GraphicsContext &context) -> Error {
  ZoneScoped;

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

  renderingInfo.renderArea.offset = {.x = 0, .y = 0};
  renderingInfo.renderArea.extent = GetRenderExtent(context, *TopOfStack);
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  if (TopOfStack->renderTargets.size() >
      context.deviceProperties.limits.maxColorAttachments) {
    return Error::Create(
        "Number of bound render targets exceeds device limits.");
  }

  if (renderingInfo.renderArea.extent.width == 0 ||
      renderingInfo.renderArea.extent.height == 0) {
    return Error::Create("Render area has zero width or height.");
  }

  thread_local auto colorAttachments = std::vector<VkRenderingAttachmentInfo>{};
  thread_local auto depthAttachment = VkRenderingAttachmentInfo{};
  thread_local auto stencilAttachment = VkRenderingAttachmentInfo{};

  colorAttachments.clear();

  bool hasDepth = false;
  bool hasStencil = false;

  for (const auto &rendertarget : TopOfStack->renderTargets) {
    auto useResult = rendertarget.texture->UseAsAttachment(context);
    if (Error::IsError(useResult)) {
      return useResult;
    }

    thread_local VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentInfo.loadOp = rendertarget.loadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    if (Image::IsDepthTexture(rendertarget.texture->format)) {
      if (hasDepth) {
        return Error::Create("Multiple depth attachments bound.");
      }
      depthAttachment = attachmentInfo;
      depthAttachment.imageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      hasDepth = true;

      Barrier::UpdateUsage(
          context, *rendertarget.texture,
          {.stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
           .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT});
    } else if (Image::IsStencilTexture(rendertarget.texture->format)) {
      if (hasStencil) {
        return Error::Create("Multiple stencil attachments bound.");
      }
      stencilAttachment = attachmentInfo;
      stencilAttachment.imageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      hasStencil = true;

      Barrier::UpdateUsage(
          context, *rendertarget.texture,
          {.stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
           .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT});
    } else {
      colorAttachments.emplace_back(attachmentInfo);

      Barrier::UpdateUsage(
          context, *rendertarget.texture,
          {.stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
           .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT});
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

  if (Graphics::GetCommandBuffer() == VK_NULL_HANDLE) {
    return Error::Create(
        "Tried to begin rendering, but command buffer is null.");
  }

  vkCmdBeginRendering(Graphics::GetCommandBuffer(), &renderingInfo);
  GetIsCurrentlyRendering() = true;

  UsedShaderModules.emplace_back(TopOfStack->shader);

  for (auto &rendertarget : TopOfStack->renderTargets) {
    // Make sure subsequent renders load from the existing content if we ever need to re-bind mid-pass
    rendertarget.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  return Error::Success();
}

auto EndRendering(const GraphicsContext &context) -> void {
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto InsertResourceBarriers(const GraphicsContext &context) -> Error {
  ZoneScoped;

  auto &shader = TopOfStack->shader;

  for (const auto &bufferPair : shader->GetState().userBoundBuffers) {
    const auto &buffer = bufferPair.second;
    auto key = bufferPair.first;

    if (key == 0U && shader->reflection.hasGlobals) {
      continue; // Skip slot 0, 0; set, binding = 0, 0; ubo.
    }

    const auto *slotInfo = shader->GetSlotDescription(key);
    if (slotInfo == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound buffer slot.");
    }
    if (!slotInfo->Is<Reflect::BufferInfo>()) {
      return Error::Create("Expected buffer info for bound buffer slot.");
    }

    const auto &info = slotInfo->GetInfo<Reflect::BufferInfo>();

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
    default:
      break;
    }

    if (access == 0 && info.access != SLANG_RESOURCE_ACCESS_NONE) {
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

    Barrier::UpdateUsage(context, *buffer.first,
                         {
                             .stages = stages,
                             .access = access,
                         });
  }

  for (auto &texturePair : shader->GetState().userBoundTextures) {
    auto &texture = texturePair.second;
    auto key = texturePair.first;

    const auto *infoResult = shader->GetSlotDescription(key);
    if (infoResult == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound texture slot.");
    }

    const auto &info = infoResult->GetInfo<Reflect::SamplerInfo>();

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
    default:
      break;
    }

    if (access == 0) {
      PrintWarning("Texture access type is Unknown for slang access: {}, "
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

    Barrier::UpdateUsage(context, *texture.first,
                         {
                             .stages = stages,
                             .access = access,
                         });
  }

  return Error::Success();
}

inline auto BindBufferDesciptors(DescriptorKey &key, auto &shader,
                                 const Shader::BoundState &state, int setIndex)
    -> Error {
  ZoneScoped;

  for (const auto &pair : state.userBoundBuffers) {
    const auto location = pair.first;
    const auto &buffer = pair.second;
    const auto &[set, binding] = Utils::SlotToSetBinding(location);

    if (set != setIndex || !buffer.first.isValid()) {
      continue;
    }

    thread_local VkDescriptorType descriptorType;
    descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;

    const auto *info = shader->GetSlotDescription(location);
    if (info == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound buffer slot.");
    }
    if (!info->template Is<Reflect::BufferInfo>()) {
      return Error::Create("Expected buffer info for bound buffer slot.");
    }
    auto *bufferInfo = info->template GetInfoPtr<Reflect::BufferInfo>();

    if (bufferInfo->bufferType == Reflect::BufferType::Uniform) {
      descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    } else if (bufferInfo->bufferType == Reflect::BufferType::Storage) {
      descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    } else {
      return Error::Create("Unknown buffer type for descriptor set binding.");
    }

    key.bindings.emplace_back(ResourceBinding{
        .binding = binding,
        .descriptorType = descriptorType,
        .resourceInfo =
            VkDescriptorBufferInfo{
                .buffer = buffer.first->handle,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
    });
  }

  return Error::Success();
}

inline auto BindTextureDescriptors(const GraphicsContext &context,
                                   VkPipelineStageFlags2 stage,
                                   DescriptorKey &key,
                                   Ref<Shader::ShaderModule> &shader,
                                   int setIndex) -> Error {
  ZoneScoped;

  for (const auto &pair : shader->GetState().userBoundTextures) {
    const auto location = pair.first;
    const auto &texture = pair.second.first;
    const auto *samplerInfo = pair.second.second;
    const auto &[set, binding] = Utils::SlotToSetBinding(location);

    if (set != setIndex) {
      continue;
    }

    auto usage = samplerInfo->access == SLANG_RESOURCE_ACCESS_READ
                     ? TextureUsage::Sampler
                     : TextureUsage::Storage;

    Error result;
    switch (usage) {
    case TextureUsage::Sampler:
      result = texture->UseAsSampler(context, stage);
      break;
    case TextureUsage::Storage:
      result = texture->UseAsStorage(context, stage);
      break;
    case TextureUsage::Attachment:
      result = texture->UseAsAttachment(context);
      break;
    case TextureUsage::TransferSrc:
      result = texture->UseAsTransferSrc(context);
      break;
    case TextureUsage::TransferDst:
      result = texture->UseAsTransferDst(context);
      break;
    case TextureUsage::PresentSrc:
      result = texture->UseAsPresentSrc(context);
      break;
    case TextureUsage::Unknown:
      result = Error::Create(
          "Cannot transition image with unknown usage in shader flush.");
      break;
    }

    if (Error::IsError(result)) {
      return result;
    }

    key.bindings.emplace_back(ResourceBinding{
        .binding = binding,
        .descriptorType = samplerInfo->access == SLANG_RESOURCE_ACCESS_READ
                              ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                              : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .resourceInfo =
            VkDescriptorImageInfo{
                .sampler = texture->GetSampler(context),
                .imageView = texture->view,
                .imageLayout = texture->currentLayout,
            },
    });
  }

  return Error::Success();
}

inline auto BindGlobalsDescriptor(const GraphicsContext &context,
                                  DescriptorKey &key, auto &shader,
                                  int setIndex,
                                  std::vector<uint32_t> &dynamicOffsets)
    -> Error {
  ZoneScoped;

  if (!shader->reflection.hasGlobals) {
    return Error::Success();
  }

  const auto set = shader->reflection.globals.set;
  const auto binding = shader->reflection.globals.binding;

  if (set != setIndex) {
    return Error::Success();
  }

  auto &buffer = GetGlobalUniformBuffer(context.frameIndex);

  dynamicOffsets.emplace_back(buffer.GetOffset());

#if Enable_Snapshots
  Snapshot::CaptureEvent(Snapshot::StructuredBufferUploadEvent(
      buffer.GetBuffer()->handle, shader->reflection.globalBufferFormat));
#endif

  auto flushResult = buffer.Write(context, shader->globalUniforms);

  if (Error::IsError(flushResult)) {
    return flushResult.error();
  }

  key.bindings.emplace_back(ResourceBinding{
      .binding = binding,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
      .resourceInfo =
          VkDescriptorBufferInfo{
              .buffer = buffer.GetBuffer()->handle,
              .offset = 0,
              .range = Utils::AlignUp(shader->reflection.globals.size,
                                      context.deviceProperties.limits
                                          .minUniformBufferOffsetAlignment),
          },
  });

  return Error::Success();
}

inline auto AllocateDescriptorSets(const GraphicsContext &context,
                                   DescriptorKey &key,
                                   const VkDescriptorSetLayout &layout)
    -> Result<VkDescriptorSet> {
  ZoneScoped;

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = GetThreadContext().descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;
  VkDescriptorSet descriptorSet = nullptr;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    CHECK_ERR(Error::Create(
        vkAllocateDescriptorSets(context.device, &allocInfo, &descriptorSet)));
  }

  thread_local std::vector<VkWriteDescriptorSet> writeDescriptorSets;
  thread_local std::vector<VkDescriptorBufferInfo> bufferInfos;
  thread_local std::vector<VkDescriptorImageInfo> imageInfos;

  writeDescriptorSets.clear();
  bufferInfos.clear();
  imageInfos.clear();

  writeDescriptorSets.reserve(key.bindings.size());
  bufferInfos.reserve(key.bindings.size());
  imageInfos.reserve(key.bindings.size());

  for (const auto &binding : key.bindings) {
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = binding.binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = binding.descriptorType;

    // clang-format off
        if (std::holds_alternative<VkDescriptorBufferInfo>(binding.resourceInfo)) {
          bufferInfos.emplace_back(std::get<VkDescriptorBufferInfo>(binding.resourceInfo));
          write.pBufferInfo = &bufferInfos.back();

        } else if (std::holds_alternative<VkDescriptorImageInfo>(binding.resourceInfo)) {
          imageInfos.emplace_back(std::get<VkDescriptorImageInfo>(binding.resourceInfo));
          write.pImageInfo = &imageInfos.back();
        } else {
          return Error::Unexpected("Unknown resource info type in descriptor set binding.");
        }
    // clang-format on

    writeDescriptorSets.emplace_back(write);
  }

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    vkUpdateDescriptorSets(context.device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(), 0, nullptr);
  }

  assert(CurrentPipelineLayout.layout != nullptr);
  assert(descriptorSet != VK_NULL_HANDLE);

  return descriptorSet;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto BindDescriptorSets(const GraphicsContext &context,
                        VkPipelineStageFlags2 stage) -> Error {
  ZoneScoped;
  auto &shader = TopOfStack->shader;
  auto &state = shader->GetState();

  VkPipelineStageFlags2 stageFlags = 0;

  if (TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    stageFlags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    stageFlags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  } else if (TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    stageFlags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }

  int setIndex = 0;
  for (const auto &layout : CurrentPipelineLayout.descriptorSetLayouts) {
    thread_local DescriptorKey key = {
        .layout = layout,
        .bindings = {},
    };
    key.bindings.clear();

    thread_local std::vector<uint32_t> dynamicOffsets;
    dynamicOffsets.clear();

    CHECK_ERR(BindBufferDesciptors(key, shader, state, setIndex));
    CHECK_ERR(
        BindTextureDescriptors(context, stageFlags, key, shader, setIndex));
    CHECK_ERR(
        BindGlobalsDescriptor(context, key, shader, setIndex, dynamicOffsets));

    auto iter = DescriptorSetCache.find(key);

    auto *commandBuffer = Graphics::GetCommandBuffer();

    if (commandBuffer == VK_NULL_HANDLE) {
      return Error::Create("Command buffer is null in BindDescriptorSets.");
    }

    if (iter != DescriptorSetCache.end()) {
      ZoneScopedN("vkCmdBindDescriptorSets cached");

      vkCmdBindDescriptorSets(commandBuffer, TopOfStack->bindPoint,
                              CurrentPipelineLayout.layout, setIndex, 1,
                              &iter->second, dynamicOffsets.size(),
                              dynamicOffsets.data());
    } else {
      ZoneScopedN("Allocate and bind descriptor set");

      auto *descriptorSet =
          CHECK_RES(AllocateDescriptorSets(context, key, layout));

      vkCmdBindDescriptorSets(commandBuffer, TopOfStack->bindPoint,
                              CurrentPipelineLayout.layout, setIndex, 1,
                              &descriptorSet, dynamicOffsets.size(),
                              dynamicOffsets.data());

      DescriptorSetCache[key] = descriptorSet;
    }

    setIndex++;
  }

  return Error::Success();
}

auto compareScissors(const State &first, const State &second) -> bool {
  if (first.hasScissor != second.hasScissor) {
    return false;
  }

  if (first.hasScissor && second.hasScissor) {
    if (first.scissor.offset.x != second.scissor.offset.x ||
        first.scissor.offset.y != second.scissor.offset.y ||
        first.scissor.extent.width != second.scissor.extent.width ||
        first.scissor.extent.height != second.scissor.extent.height) {
      return false;
    }
  }

  return true;
}

auto compareViewports(const State &first, const State &second) -> bool {
  if (first.hasViewport != second.hasViewport) {
    return false;
  }

  if (first.hasViewport && second.hasViewport) {
    if (first.viewport.x != second.viewport.x ||
        first.viewport.y != second.viewport.y ||
        first.viewport.width != second.viewport.width ||
        first.viewport.height != second.viewport.height) {
      return false;
    }
  }

  return true;
}

auto PrepareRendering(const GraphicsContext &context) -> Error {
  ZoneScoped;

  // Flush updates the last state so we need to compare before updating it
  bool sameViewport =
      LastState != nullptr && compareViewports(*TopOfStack, *LastState);
  bool sameScissor =
      LastState != nullptr && compareScissors(*TopOfStack, *LastState);

  auto flushResult = Flush(context);

  if (Error::IsError(flushResult)) {
    return flushResult.error();
  }

  auto updatedState = flushResult.value();

  {
    assert(CurrentPipelineLayout.layout != nullptr);
    assert(GetCommandBuffer() != VK_NULL_HANDLE);
    ZoneScopedN("Flush push buffer data");
    for (auto &pushBuffer : TopOfStack->shader->pushBuffers) {
      FlushInfo info{
          .commandBuffer = GetCommandBuffer(),
          .pipelineLayout = CurrentPipelineLayout.layout,
      };

      pushBuffer.FlushData(info);
    }
  }

  auto insertionResult = InsertResourceBarriers(context);

  if (Error::IsError(insertionResult)) {
    return insertionResult;
  }

  auto stages = VK_PIPELINE_STAGE_2_NONE;

  for (const auto &stage : TopOfStack->shader->stages) {
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

  auto bindDefaultResult =
      BindDefaultTextures(context, TopOfStack->shader.get());
  if (Error::IsError(bindDefaultResult)) {
    return bindDefaultResult;
  }

  auto bindResult = BindDescriptorSets(context, stages);
  if (Error::IsError(bindResult)) {
    return bindResult;
  }

  bool wasRendering = GetIsCurrentlyRendering();

  if (TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS &&
      !wasRendering) {
    auto beginResult = BeginRendering(context);

    if (Error::IsError(beginResult)) {
      return beginResult;
    }
  }

  if ((TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS &&
       !sameViewport) ||
      !wasRendering) {
    ZoneScopedN("Set Viewport And Scissor");

    auto viewport = GetClippedViewport();
    vkCmdSetViewport(Graphics::GetCommandBuffer(), 0, 1, &viewport);
  }

  if ((TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS &&
       !sameScissor) ||
      !wasRendering) {
    ZoneScopedN("Set Scissor");

    auto scissor = GetScissor();
    vkCmdSetScissor(Graphics::GetCommandBuffer(), 0, 1, &scissor);
  }

  Graphics::GetIsStateDirty() = false;
  return Error::Success();
}

auto FinalizeFrame(const GraphicsContext &context) -> Error {
  ZoneScoped;
  if (StateStack.size() != 1) {
    return Error::Create("More pushes than pops.");
  }
  if (TopOfStack->renderTargets.size() != 0) {
    for (const auto &rendertarget : TopOfStack->renderTargets) {
      if (!IsSwapchainTexture(context, *rendertarget.texture)) {
        return Error::Create(
            "Non-swapchain render targets remain bound at end of frame.");
      }
    }
  }
  EndRendering(context);
  return Error::Success();
}

auto UsedInPass(const Texture &texture) -> bool {
  return std::ranges::any_of(TopOfStack->renderTargets,
                             [&](const auto &target) -> auto {
                               return target.texture->image == texture.image;
                             });
}

auto BeginFrame(const GraphicsContext &context) -> Error {
  // Setup stack with new swapchain texture

  StateStack.clear();

  auto state = CHECK_RES(SetupDefaultState(context));
  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();
  DrawnToSwapchain = false;

  UsedShaderModules.clear();

  return Error::Success();
}

// Setters //
auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp)
    -> void {
  TopOfStack->depthTestEnable = enable;
  TopOfStack->depthWriteEnable = writeEnable;
  TopOfStack->depthCompareOp = compareOp;

  TopOfStack->dirty = true;
}

auto SetCullMode(VkCullModeFlags cullMode) -> void {
  TopOfStack->cullMode = cullMode;
  TopOfStack->dirty = true;
}

auto SetPolygonMode(VkPolygonMode polygonMode) -> void {
  TopOfStack->polygonMode = polygonMode;
  TopOfStack->dirty = true;
}

auto SetViewport(const VkViewport *viewport) -> void {
  TopOfStack->dirty = true;
  if (viewport == nullptr) {
    TopOfStack->hasViewport = false;
    return;
  }

  TopOfStack->hasViewport = true;
  TopOfStack->viewport = *viewport;
}

auto SetScissor(const VkRect2D *scissor) -> void {
  TopOfStack->dirty = true;
  if (scissor == nullptr) {
    TopOfStack->hasScissor = false;
    return;
  }

  TopOfStack->hasScissor = true;
  TopOfStack->scissor = *scissor;
}

auto ClipScissor(const VkRect2D &scissor) -> void {
  TopOfStack->dirty = true;
  auto &currentScissor = TopOfStack->scissor;

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
  TopOfStack->dirty = true;
  if (shader.get() == nullptr) {
    TopOfStack->shader = Shader::DefaultShaderModule;
  } else {
    TopOfStack->shader = shader;
  }
}

auto SetRenderTargets(const GraphicsContext &context,
                      const std::vector<RenderTarget> &renderTargets) -> Error {
  TopOfStack->dirty = true;

  if (renderTargets.empty()) {
    return Error::Create("No render targets provided.");
  }

  bool differentFromCurrent =
      renderTargets.size() != TopOfStack->renderTargets.size();

  for (const auto &target : renderTargets) {
    if (!differentFromCurrent) {
      auto iter = std::ranges::find_if(
          TopOfStack->renderTargets, [&](const auto &currentTarget) -> bool {
            return currentTarget.texture->image == target.texture->image;
          });

      if (iter == TopOfStack->renderTargets.end()) {
        differentFromCurrent = true;
      }
    }
  }

  TopOfStack->renderTargets = renderTargets;
  SetViewport(nullptr);

  // Only clear if we have the same render targets as before
  // And have load ops that require clearing
  if (differentFromCurrent) {
    return Error::Success();
  }

  if (GetIsCurrentlyRendering()) {
    ClearInfo clearInfo{};
    for (const auto &target : renderTargets) {
      if (target.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        if (target.texture->IsDepthTexture()) {
          clearInfo.clearDepth = true;
          clearInfo.depthClearValue = target.clearValue.depthStencil.depth;
        } else if (target.texture->IsStencilTexture()) {
          clearInfo.clearStencil = true;
          clearInfo.stencilClearValue =
              static_cast<int>(target.clearValue.depthStencil.stencil);
        } else {
          clearInfo.colors.emplace_back(target.clearValue.color.float32[0],
                                        target.clearValue.color.float32[1],
                                        target.clearValue.color.float32[2],
                                        target.clearValue.color.float32[3]);
        }
      }
    }

    auto clearResult = Clear(context, clearInfo);
    if (Error::IsError(clearResult)) {
      return clearResult;
    }
  }

  return Error::Success();
}

auto SetWindingOrder(VkFrontFace frontFace) -> void {
  TopOfStack->frontFace = frontFace;
  TopOfStack->dirty = true;
}

auto SetTopology(VkPrimitiveTopology topology) -> void {
  TopOfStack->primitiveTopology = topology;
  TopOfStack->dirty = true;
}

// Getters //

auto GetDepthMode() -> std::tuple<bool, bool, VkCompareOp> {
  return {TopOfStack->depthTestEnable, TopOfStack->depthWriteEnable,
          TopOfStack->depthCompareOp};
}

auto GetCullMode() -> VkCullModeFlags { return TopOfStack->cullMode; }

auto GetPolygonMode() -> VkPolygonMode { return TopOfStack->polygonMode; }

auto GetMaximumAllowedViewport() -> VkViewport {
  auto viewport = TopOfStack->viewport;

  auto size = TopOfStack->renderTargets[0].texture->size;

  viewport.width = static_cast<float>(size.width);
  viewport.height = static_cast<float>(size.height);

  viewport.width = (std::max)(viewport.width, 1.0F);
  viewport.height = (std::max)(viewport.height, 1.0F);

  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  return viewport;
}

auto GetClippedViewport() -> VkViewport {

  if (!TopOfStack->hasViewport) {
    return GetMaximumAllowedViewport();
  }

  auto viewport = TopOfStack->viewport;

  // Default to size of current attachments
  auto size = TopOfStack->renderTargets[0].texture->size;

  viewport.width = std::min(viewport.width, static_cast<float>(size.width));
  viewport.height = std::min(viewport.height, static_cast<float>(size.height));

  return viewport;
}

auto GetViewport() -> VkViewport {

  if (!TopOfStack->hasViewport) {
    return GetMaximumAllowedViewport();
  }

  // return TopOfStack->viewport;
  return {
      .x = TopOfStack->viewport.x,
      .y = TopOfStack->viewport.y,
      .width = (std::max)(TopOfStack->viewport.width, 1.0F),
      .height = (std::max)(TopOfStack->viewport.height, 1.0F),
      .minDepth = TopOfStack->viewport.minDepth,
      .maxDepth = TopOfStack->viewport.maxDepth,
  };
}

auto GetScissor() -> VkRect2D {

  if (!TopOfStack->hasScissor) {
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

  // return TopOfStack->scissor;

  return {
      .offset =
          {
              .x = TopOfStack->scissor.offset.x,
              .y = TopOfStack->scissor.offset.y,
          },
      .extent =
          {
              .width = (std::max)(TopOfStack->scissor.extent.width, 1U),
              .height = (std::max)(TopOfStack->scissor.extent.height, 1U),
          },
  };
}

auto GetShader() -> Ref<Shader::ShaderModule> {
  if (TopOfStack->shader.get() == Shader::DefaultShaderModule.get()) {
    return Ref<Shader::ShaderModule>(nullptr);
  }
  return TopOfStack->shader;
}

auto GetRenderTargets() -> std::vector<RenderTarget> {
  return TopOfStack->renderTargets;
}

auto GetWindingOrder() -> VkFrontFace { return TopOfStack->frontFace; }

auto GetTopology() -> VkPrimitiveTopology {
  return TopOfStack->primitiveTopology;
}

auto SetBindPoint(VkPipelineBindPoint bindPoint) -> void {
  TopOfStack->bindPoint = bindPoint;
}
auto GetBindPoint() -> VkPipelineBindPoint { return TopOfStack->bindPoint; }

auto Clear(const GraphicsContext &context, const ClearInfo &clearInfo)
    -> Error {
  ZoneScoped;

  auto *commandBuffer = Graphics::GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Command buffer is null in Clear.");
  }

  auto count = TopOfStack->renderTargets.size();

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
    auto &rendertarget = TopOfStack->renderTargets[i];

    if (Image::IsDepthTexture(rendertarget.texture->format) ||
        Image::IsStencilTexture(rendertarget.texture->format)) {
      clearAttachment.aspectMask = 0;
      bool doClear = false;
      if (Image::IsDepthTexture(rendertarget.texture->format)) {
        clearAttachment.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        doClear = clearInfo.clearDepth;
      }
      if (Image::IsStencilTexture(rendertarget.texture->format)) {
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
  CHECK_ERR(PrepareRendering(context));

  vkCmdClearAttachments(
      commandBuffer, static_cast<uint32_t>(clearAttachments.size()),
      clearAttachments.data(), static_cast<uint32_t>(clearRects.size()),
      clearRects.data());

  return Error::Success();
}

auto State::GetHash() const -> uint64_t {
  if (dirty) {
    hash = StateHash::Hash(*this);
    dirty = false;
  }

  return hash;
}

auto RenderTarget::GetHash() const -> uint64_t {
  if (dirty) {
    hash = HashRenderTarget(*this);
    dirty = false;
  }

  return hash;
}

} // namespace Graphics::DynamicRendering
