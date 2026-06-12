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
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
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

LRUCache<StateKey, std::pair<VkPipeline, PipelineLayout>, StateKeyHash>
    PipelineCache(PipelineCacheSize);

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

thread_local Stats CurrentStats;

VkDescriptorSetLayout DefaultEmptySetLayout = VK_NULL_HANDLE;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto GetRenderExtent(const GraphicsContext &context, const State &state)
    -> VkExtent2D {

  if (state.colorAttachmentCount > 0) {
    return VkExtent2D{
        .width = state.colorAttachments.at(0).texture->size.width,
        .height = state.colorAttachments.at(0).texture->size.height,
    };
  }

  if (state.hasDepthStencilAttachment) {
    return VkExtent2D{
        .width = state.depthStencilAttachment.texture->size.width,
        .height = state.depthStencilAttachment.texture->size.height,
    };
  }

  assert(false && "Trying to get render extent with no attachments.");
  return VkExtent2D{0, 0};
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

  std::unordered_set<uint32_t> uniqueSets{};
  bool hasBindings = false;
  for (const auto &setPair : shader->bindingInfos) {
    uniqueSets.insert(setPair.first);
    hasBindings = true;
  }

  uint32_t setCount = uniqueSets.size();
  uint32_t maxSet = 0;
  auto maxSetIter = std::ranges::max_element(uniqueSets);
  if (maxSetIter != uniqueSets.end()) {
    maxSet = std::max(setCount - 1, *maxSetIter);
  }

  // PrintAlways("Max descriptor set index from shader reflection: {}", maxSet);

  setLayouts.resize(maxSet + 1, DefaultEmptySetLayout);

  // For each set, build the DescriptorSetLayoutKey and get the layout
  for (const auto &setPair : shader->bindingInfos) {
    DescriptorSetLayoutKey layoutKey;
    layoutKey.flags = 0;
    layoutKey.bindings = setPair.second;
    layoutKey.bindingFlags = {};

    const auto &layout = CHECK_RES(GetDescriptorSetLayout(layoutKey, context));
    // setLayouts.emplace_back(layout);
    setLayouts.at(setPair.first) = layout;
  }

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = setLayouts.size();
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

      auto defaultTexture = CHECK_RES(GetDefaultTexture(context, format, type));
      state.userBoundTextures[key] = {defaultTexture, &samplerInfo};
    }
  }

  return Error::Success();
}

inline auto GetShaderStages(const State &state)
    -> Result<std::vector<VkPipelineShaderStageCreateInfo>> {
  ZoneScoped;

  thread_local std::unordered_map<Shader::ShaderModule *,
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

  // PrintAlways("Shader: {}", shader->name);
  for (const auto &stage : shader->entryPoints) {
    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage.second;
    stageInfo.module = shader->module;
    stageInfo.pName = stage.first.c_str();
    // PrintAlways("Entry Point: {}, Stage: {}", stage.first, (int)stage.second);

    shaderStages.emplace_back(stageInfo);
  }

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

#ifndef NDEBUG
  if (!shader->entryPointToStageIndex.contains(
          SlangStage::SLANG_STAGE_FRAGMENT)) {
    return std::vector<VkPipelineColorBlendAttachmentState>{};
  }

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
    // No fragment outputs, so no blend attachments needed
    return std::vector<VkPipelineColorBlendAttachmentState>{};
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
#endif

  // Get Actual Set Render Targets //

  auto blendAttachments = std::vector<VkPipelineColorBlendAttachmentState>();
  blendAttachments.resize(MAX_COLOR_ATTACHMENTS);

  bool hasImplicitLocation = false;
  bool hasExplicitLocation = false;

  for (int i = 0; i < state.colorAttachmentCount; ++i) {
    const auto &rendertarget = state.colorAttachments.at(i);
    int location = rendertarget.location;
    if (location == -1) {
      location = i;
      hasImplicitLocation = true;
    } else {
      hasExplicitLocation = true;
    }

    if (location < 0 || location >= MAX_COLOR_ATTACHMENTS) {
      return Error::Unexpected("Render target location is out of bounds: " +
                               std::to_string(location));
    }

    blendAttachments.at(location) = rendertarget.blendMode;
  }

  if (hasImplicitLocation && hasExplicitLocation) {
    return Error::Unexpected(
        "Cannot mix implicit and explicit render target locations.");
  }

#ifndef NDEBUG
  for (uint32_t i = 0; i <= blendAttachments.size(); ++i) {
    if (expectedAttachments.contains(i)) {
      if (i >= blendAttachments.size()) {
        return Error::Unexpected(
            "Missing blend attachment for expected output location " +
            std::to_string(i));
      }
    }
  }
#endif

  return blendAttachments;
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

  // NOLINTNEXTLINE
  std::array<VkDynamicState, 11> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,

      VK_DYNAMIC_STATE_CULL_MODE,
      VK_DYNAMIC_STATE_FRONT_FACE,

      VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
      VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
      VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,

      VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
      VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
      VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
  };

  dynamicState.pDynamicStates = dynamicStates.data();
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());

  /// Depth and Stencil ///

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthTestEnable = state.depthTestEnable;
  depthStencilState.depthWriteEnable = state.depthWriteEnable;
  depthStencilState.depthCompareOp = state.depthCompareOp;
  depthStencilState.depthBoundsTestEnable = VK_FALSE;
  depthStencilState.stencilTestEnable = state.stencilTestEnable;

  /// Attachment Formats ///

  auto formats = std::array<VkFormat, MAX_COLOR_ATTACHMENTS>();
  VkFormat depthFormat = VK_FORMAT_UNDEFINED;
  VkFormat stencilFormat = VK_FORMAT_UNDEFINED;

  if (state.hasDepthStencilAttachment) {
    auto &rendertarget = state.depthStencilAttachment;

    if (rendertarget.texture->IsDepthTexture()) {
      depthFormat = rendertarget.texture->format;
    }
    if (rendertarget.texture->IsStencilTexture()) {
      stencilFormat = rendertarget.texture->format;
    }
  }

  bool hasImplicitLocation = false;
  bool hasExplicitLocation = false;

  for (int i = 0; i < state.colorAttachmentCount; i++) {
    const auto &rendertarget = state.colorAttachments.at(i);

    int location = rendertarget.location;
    if (location == -1) {
      location = i;
      hasImplicitLocation = true;
    } else {
      hasExplicitLocation = true;
    }

    formats.at(location) = rendertarget.texture->format;
  }

  if (hasImplicitLocation && hasExplicitLocation) {
    return Error::Unexpected(
        "Cannot mix explicit and implicit render target locations.");
  }

  /// Color Attachments ///

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = state.colorAttachmentCount;
  renderingCreateInfo.pColorAttachmentFormats = formats.data();
  renderingCreateInfo.depthAttachmentFormat = depthFormat;
  renderingCreateInfo.stencilAttachmentFormat = stencilFormat;

  auto blendAttachments =
      CHECK_RES(GetColorBlendAttachmentState(context, state));

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = state.colorAttachmentCount;
  colorBlending.pAttachments = blendAttachments.data();

  if (state.colorAttachmentCount == 0) {
    colorBlending.pAttachments = nullptr;
  }

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

  auto key = StateKey(state);
  PipelineCache.emplace(key, std::make_pair(pipeline, layout));

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

  // If we get a validation error / crash about this not existing; rename to "main"
  pipelineInfo.stage.pName = "computeMain";

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

  auto key = StateKey(state);
  PipelineCache.emplace(key, std::make_pair(pipeline, layout));

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

  auto *pipeline = PipelineCache.get(StateKey(state));

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

  defaultState.colorAttachments.at(0) = swapchainRendertarget;
  defaultState.colorAttachmentCount = 1;
  defaultState.hasDepthStencilAttachment = false;

  return defaultState;
}

auto Load(const GraphicsContext &context) -> Error {
  assert(StateStack.size() == 0 &&
         "RenderTarget state stack is not empty on Load.");

  auto state = CHECK_RES(SetupDefaultState(context));

  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();

  if (DefaultEmptySetLayout == VK_NULL_HANDLE) {
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 0;
    layoutInfo.pBindings = nullptr;
    CHECK_ERR(Error::Create(vkCreateDescriptorSetLayout(
        context.device, &layoutInfo, GetAllocationCallbacks(),
        &DefaultEmptySetLayout)));
  }

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
  CurrentStats.Reset();

  auto state = CHECK_RES(SetupDefaultState(context));

  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();
  LastState = nullptr;

  return Error::Success();
}

auto Shutdown(const GraphicsContext &context) -> Error {
  StateStack.clear();
  CurrentStats.Reset();
  LastState = nullptr;
  TopOfStack = nullptr;
  PipelineCache.clear();
  LastStateStorage = State();

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

  assert(TopOfStack->shader->entryPoints.at(0).second ==
         VK_SHADER_STAGE_COMPUTE_BIT);

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

  for (int i = 0; i < TopOfStack->colorAttachmentCount; ++i) {
    const auto &rendertarget = TopOfStack->colorAttachments.at(i);
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

  CurrentStats.contextSwitches++;

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

  vkDestroyDescriptorSetLayout(context.device, DefaultEmptySetLayout,
                               GetAllocationCallbacks());

  Pipelines.clear();
  PipelineLayouts.clear();
  DynamicRendering::DescriptorSetLayoutCache.clear();
}

// NOLINTNEXTLINE
inline auto BeginRendering(const GraphicsContext &context) -> Error {
  ZoneScoped;

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

  renderingInfo.renderArea.offset = {.x = 0, .y = 0};
  renderingInfo.renderArea.extent = GetRenderExtent(context, *TopOfStack);
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  if (TopOfStack->colorAttachmentCount >
      context.deviceProperties.limits.maxColorAttachments) {
    return Error::Create(
        "Number of bound render targets exceeds device limits.");
  }

  if (renderingInfo.renderArea.extent.width == 0 ||
      renderingInfo.renderArea.extent.height == 0) {
    return Error::Create("Render area has zero width or height.");
  }

  auto colorAttachments =
      std::array<VkRenderingAttachmentInfo, MAX_COLOR_ATTACHMENTS>{};
  auto depthAttachment = VkRenderingAttachmentInfo{};
  auto stencilAttachment = VkRenderingAttachmentInfo{};

  for (int i = 0; i < TopOfStack->colorAttachmentCount; i++) {
    const auto &rendertarget = TopOfStack->colorAttachments.at(i);
    CHECK_ERR(rendertarget.texture->UseAsAttachment(context));

    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentInfo.loadOp = rendertarget.loadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    colorAttachments.at(i) = attachmentInfo;

    Barrier::UpdateUsage(
        context, *rendertarget.texture,
        {.stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
         .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                   VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT});
  }

  bool hasDepth = false;
  bool hasStencil = false;

  if (TopOfStack->hasDepthStencilAttachment) {
    auto &rendertarget = TopOfStack->depthStencilAttachment;
    CHECK_ERR(rendertarget.texture->UseAsAttachment(context));

    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentInfo.loadOp = rendertarget.loadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    if (rendertarget.texture->IsDepthTexture()) {
      depthAttachment = attachmentInfo;
      hasDepth = true;
    }

    if (rendertarget.texture->IsStencilTexture()) {
      stencilAttachment = attachmentInfo;
      hasStencil = true;
    }

    Barrier::UpdateUsage(
        context, *rendertarget.texture,
        {.stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
         .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT});
  }

#ifndef NDEBUG
  VkExtent2D expectedExtent = GetRenderExtent(context, *TopOfStack);

  for (int i = 0; i < TopOfStack->colorAttachmentCount; i++) {
    const auto &rendertarget = TopOfStack->colorAttachments.at(i);
    if (rendertarget.texture->size.width != expectedExtent.width ||
        rendertarget.texture->size.height != expectedExtent.height) {
      return Error::Create(
          "Color attachment extent does not match render area extent.");
    }
  }

  if (hasDepth || hasStencil) {
    if (TopOfStack->depthStencilAttachment.texture->size.width !=
            expectedExtent.width ||
        TopOfStack->depthStencilAttachment.texture->size.height !=
            expectedExtent.height) {
      return Error::Create(
          "Depth/stencil attachment extent does not match render area extent.");
    }
  }
#endif

  renderingInfo.colorAttachmentCount = TopOfStack->colorAttachmentCount;
  renderingInfo.pColorAttachments = colorAttachments.data();

  renderingInfo.pStencilAttachment = hasStencil ? &stencilAttachment : nullptr;
  renderingInfo.pDepthAttachment = hasDepth ? &depthAttachment : nullptr;

  if (Graphics::GetCommandBuffer() == VK_NULL_HANDLE) {
    return Error::Create(
        "Tried to begin rendering, but command buffer is null.");
  }

  // Add a tracy marker to indicate the start of a rendering pass
  TracyMessageL("Begin Rendering");
  vkCmdBeginRendering(Graphics::GetCommandBuffer(), &renderingInfo);
  GetIsCurrentlyRendering() = true;

  for (int i = 0; i < TopOfStack->colorAttachmentCount; i++) {
    auto &rendertarget = TopOfStack->colorAttachments.at(i);
    // Make sure subsequent renders load from the existing content if we ever need to re-bind mid-pass
    rendertarget.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  if (TopOfStack->hasDepthStencilAttachment) {
    TopOfStack->depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  return Error::Success();
}

auto EndRendering(const GraphicsContext &context) -> void {
  ZoneScoped;
  if (GetIsCurrentlyRendering()) {
    if (Graphics::GetCommandBuffer() == VK_NULL_HANDLE) {
      PrintWarning(
          "Tried to end rendering, but command buffer is null. Skipping.");
      return;
    }

    TracyMessageL("End Rendering");
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

    for (const auto &stage : shader->entryPoints) {
      switch (stage.second) {
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

    // PrintAlways("Binding buffer with binding {} for shader: {}", binding,
    //             shader->name);
    // PrintAlways(" - Buffer Name: {} Descriptor Type: {}, Access: {}, set: {}, "
    //             "binding: {}",
    //             buffer.first->debugName, (int)descriptorType,
    //             (int)bufferInfo->access, set, binding);

    if ((buffer.first->usage & VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT) != 0) {
      if (descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        return Error::Createf("Buffer usage {} does not match expected "
                              "descriptor type of Uniform Buffer.",
                              (int)descriptorType);
      }
    } else if ((buffer.first->usage & VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT) !=
               0) {
      if (descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        return Error::Createf("Buffer usage {} does not match expected "
                              "descriptor type of Storage Buffer.",
                              (int)descriptorType);
      }
    } else {
      return Error::Create(
          "Buffer usage does not support expected descriptor type.");
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

  CHECK_RES(buffer.Write(context, shader->globalUniforms));
  assert((buffer.GetBuffer()->usage & VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT) ==
         0);

  // PrintAlways("Binding globals buffer with binding {} for shader: {}", binding,
  //             shader->name);

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

  thread_local std::vector<std::pair<void *, std::vector<uint32_t>>>
      BoundDescriptorSets{};
  thread_local VkCommandBuffer DescriptorsBoundAtCmdBuffer = VK_NULL_HANDLE;
  thread_local VkPipelineLayout DescriporsBoundAtLayout = {};

  if (DescriptorsBoundAtCmdBuffer != Graphics::GetCommandBuffer() ||
      CurrentPipelineLayout.layout != DescriporsBoundAtLayout) {
    BoundDescriptorSets.clear();
    DescriptorsBoundAtCmdBuffer = Graphics::GetCommandBuffer();
    DescriporsBoundAtLayout = CurrentPipelineLayout.layout;
  }

  int setIndex = 0;
  for (const auto &layout : CurrentPipelineLayout.descriptorSetLayouts) {
    thread_local DescriptorKey key = {
        .layout = layout,
        .bindings = {},
    };
    key.bindings.clear();
    key.layout = layout;

    thread_local std::vector<uint32_t> dynamicOffsets;
    dynamicOffsets.clear();

    CHECK_ERR(BindBufferDesciptors(key, shader, state, setIndex));
    CHECK_ERR(
        BindTextureDescriptors(context, stageFlags, key, shader, setIndex));
    CHECK_ERR(
        BindGlobalsDescriptor(context, key, shader, setIndex, dynamicOffsets));

    auto *commandBuffer = Graphics::GetCommandBuffer();

    if (commandBuffer == VK_NULL_HANDLE) {
      return Error::Create("Command buffer is null in BindDescriptorSets.");
    }

    if (BoundDescriptorSets.size() <= setIndex) {
      BoundDescriptorSets.resize(setIndex + 1);
    }

    auto &currentlyBound = BoundDescriptorSets.at(setIndex);

    VkDescriptorSet cached = VK_NULL_HANDLE;
    bool cacheHit = false;

    {
      ZoneScopedN("Check descriptor set cache");
      auto iter = DescriptorSetCache.find(key);
      cacheHit = iter != DescriptorSetCache.end();
      if (cacheHit) {
        cached = iter->second;
      }
    }

    if (cacheHit) {
      ZoneScopedN("vkCmdBindDescriptorSets cached");

      if (currentlyBound.first == cached) {
        if (currentlyBound.second == dynamicOffsets) {
          setIndex++;
          continue;
        }
      }

      currentlyBound.first = (void *)cached;
      currentlyBound.second = dynamicOffsets;

      vkCmdBindDescriptorSets(
          commandBuffer, TopOfStack->bindPoint, CurrentPipelineLayout.layout,
          setIndex, 1, &cached, dynamicOffsets.size(), dynamicOffsets.data());
    } else {
      ZoneScopedN("Allocate and bind descriptor set");

      auto *descriptorSet =
          CHECK_RES(AllocateDescriptorSets(context, key, layout));

      BoundDescriptorSets[setIndex] = {(void *)descriptorSet, dynamicOffsets};

      vkCmdBindDescriptorSets(commandBuffer, TopOfStack->bindPoint,
                              CurrentPipelineLayout.layout, setIndex, 1,
                              &descriptorSet, dynamicOffsets.size(),
                              dynamicOffsets.data());

      DescriptorSetCache[key] = descriptorSet;

      // PrintAlways("Updating descriptor sets for shader: {}",
      //             TopOfStack->shader->name);

      for (const auto &binding : key.bindings) {
        if (std::holds_alternative<VkDescriptorBufferInfo>(
                binding.resourceInfo)) {
          const auto &bufferInfo =
              std::get<VkDescriptorBufferInfo>(binding.resourceInfo);
          // PrintAlways("Binding buffer: {} to set {}, binding {}",
          // (void *)bufferInfo.buffer, setIndex, binding.binding);
        } else if (std::holds_alternative<VkDescriptorImageInfo>(
                       binding.resourceInfo)) {
          const auto &imageInfo =
              std::get<VkDescriptorImageInfo>(binding.resourceInfo);
          // PrintAlways(
          // "Binding image view: {} with sampler {} to set {}, binding {}",
          // (void *)imageInfo.imageView, (void *)imageInfo.sampler, setIndex,
          // binding.binding);
        }
      }
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

auto CompareVkPipelineColorBlendAttachmentState(
    const VkPipelineColorBlendAttachmentState &first,
    const VkPipelineColorBlendAttachmentState &second) -> bool {
  return first.blendEnable == second.blendEnable &&
         first.srcColorBlendFactor == second.srcColorBlendFactor &&
         first.dstColorBlendFactor == second.dstColorBlendFactor &&
         first.colorBlendOp == second.colorBlendOp &&
         first.srcAlphaBlendFactor == second.srcAlphaBlendFactor &&
         first.dstAlphaBlendFactor == second.dstAlphaBlendFactor &&
         first.alphaBlendOp == second.alphaBlendOp &&
         first.colorWriteMask == second.colorWriteMask;
}

auto compareDepthConfigs(const State &first, const State &second) -> bool {
  return first.depthTestEnable == second.depthTestEnable &&
         first.depthWriteEnable == second.depthWriteEnable &&
         first.depthCompareOp == second.depthCompareOp;
}

auto compareBlendmodes(const State &first, const State &second) -> bool {
  if (first.colorAttachmentCount != second.colorAttachmentCount) {
    return false;
  }

  for (size_t i = 0; i < first.colorAttachmentCount; i++) {
    if (!CompareVkPipelineColorBlendAttachmentState(
            first.colorAttachments.at(i).blendMode,
            second.colorAttachments.at(i).blendMode)) {
      return false;
    }
  }

  if (first.hasDepthStencilAttachment != second.hasDepthStencilAttachment) {
    return false;
  }

  if (first.hasDepthStencilAttachment && second.hasDepthStencilAttachment) {
    if (first.depthStencilAttachment.blendMode.blendEnable !=
        second.depthStencilAttachment.blendMode.blendEnable) {
      return false;
    }
  }

  return true;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto PrepareRendering(const GraphicsContext &context) -> Error {
  ZoneScoped;

  bool sameViewport = false;
  bool sameScissor = false;
  bool sameDepth = false;
  bool sameBlendMode = false;
  bool sameCullmode = false;
  bool sameFFWinding = false;

  // Flush updates the last state so we need to compare before updating it
  if (LastState != nullptr) {
    sameViewport = compareViewports(*TopOfStack, *LastState);
    sameScissor = compareScissors(*TopOfStack, *LastState);
    sameDepth = compareDepthConfigs(*TopOfStack, *LastState);
    sameBlendMode = compareBlendmodes(*TopOfStack, *LastState);
    sameCullmode = TopOfStack->cullMode == LastState->cullMode;
    sameFFWinding = TopOfStack->frontFace == LastState->frontFace;
  }

  auto updatedState = CHECK_RES(Flush(context));
  if (updatedState) {
    EndRendering(context);
  }

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

  CHECK_ERR(InsertResourceBarriers(context));

  auto stages = VK_PIPELINE_STAGE_2_NONE;

  for (const auto &stage : TopOfStack->shader->entryPoints) {
    switch (stage.second) {
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
  bool isGraphics = TopOfStack->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

  if (isGraphics) {
    ZoneScopedN("Dynamic state setup");

    auto *commandBuffer = Graphics::GetCommandBuffer();

    if (!wasRendering) {
      CHECK_ERR(BeginRendering(context));
      sameViewport = false;
      sameScissor = false;
      sameDepth = false;
      sameBlendMode = false;
      sameCullmode = false;
      sameFFWinding = false;
    }

    if (!sameViewport) {
      auto viewport = GetClippedViewport();
      vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    }

    if (!sameScissor) {
      auto scissor = GetScissor();
      vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    if (!sameDepth) {
      vkCmdSetDepthTestEnable(commandBuffer, TopOfStack->depthTestEnable);
      vkCmdSetDepthWriteEnable(commandBuffer, TopOfStack->depthWriteEnable);
      vkCmdSetDepthCompareOp(commandBuffer, TopOfStack->depthCompareOp);
    }

    if (!sameBlendMode && TopOfStack->colorAttachmentCount > 0) {
      std::array<VkBool32, MAX_COLOR_ATTACHMENTS> blendEnables = {};
      std::array<VkColorComponentFlags, MAX_COLOR_ATTACHMENTS> colorWriteMasks =
          {};
      int attachmentCount = TopOfStack->colorAttachmentCount;

      for (uint32_t i = 0; i < attachmentCount; i++) {
        const auto &attachment = TopOfStack->colorAttachments.at(i);
        blendEnables.at(i) = attachment.blendMode.blendEnable;
        colorWriteMasks.at(i) = attachment.blendMode.colorWriteMask;
      }

      vkCmdSetColorBlendEquationEXT(commandBuffer, 0, attachmentCount,
                                    TopOfStack->colorBlendEquations.data());
      vkCmdSetColorBlendEnableEXT(commandBuffer, 0, attachmentCount,
                                  blendEnables.data());
      vkCmdSetColorWriteMaskEXT(commandBuffer, 0, attachmentCount,
                                colorWriteMasks.data());
    }

    if (!sameCullmode) {
      vkCmdSetCullMode(commandBuffer, TopOfStack->cullMode);
    }

    if (!sameFFWinding) {
      vkCmdSetFrontFace(commandBuffer, TopOfStack->frontFace);
    }
  }

  Graphics::GetIsStateDirty() = false;
  return Error::Success();
}

auto FinalizeFrame(const GraphicsContext &context) -> Error {
  ZoneScoped;
  if (StateStack.size() != 1) {
    return Error::Create("More pushes than pops.");
  }

  for (int i = 0; i < TopOfStack->colorAttachmentCount; i++) {
    auto &rendertarget = TopOfStack->colorAttachments.at(i);
    if (!IsSwapchainTexture(context, *rendertarget.texture)) {
      return Error::Create(
          "Non-swapchain render targets remain bound at end of frame.");
    }
  }

  EndRendering(context);
  return Error::Success();
}

auto BeginFrame(const GraphicsContext &context) -> Error {
  StateStack.clear();
  CurrentStats.Reset();

  auto state = CHECK_RES(SetupDefaultState(context));
  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();
  LastState = nullptr;
  LastStateStorage = state;

  DrawnToSwapchain = false;

  return Error::Success();
}

// Setters //

auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp)
    -> void {
  TopOfStack->depthTestEnable = static_cast<VkBool32>(enable);
  TopOfStack->depthWriteEnable = static_cast<VkBool32>(writeEnable);
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto SetRenderTargets(const GraphicsContext &context,
                      const std::vector<RenderTarget> &renderTargets) -> Error {
  TopOfStack->dirty = true;

  if (renderTargets.empty()) {
    return Error::Create("No render targets provided.");
  }

  bool differentFromCurrent = false;
  bool topHadDepthStencil = TopOfStack->hasDepthStencilAttachment;
  TopOfStack->hasDepthStencilAttachment = false;

  VkExtent2D expectedExtent = {renderTargets.at(0).texture->size.width,
                               renderTargets.at(0).texture->size.height};

  int colorIdx = 0;

  for (const auto &target : renderTargets) {

    if (target.texture->size.width != expectedExtent.width ||
        target.texture->size.height != expectedExtent.height) {
      return Error::Create("All render targets must have the same dimensions.");
    }

    // Depth/Stencil is separate
    if (target.texture->IsDepthTexture() ||
        target.texture->IsStencilTexture()) {
      if (!topHadDepthStencil) {
        differentFromCurrent = true;
      }
      TopOfStack->hasDepthStencilAttachment = true;
      TopOfStack->depthStencilAttachment = target;
      continue;
    }

    if (colorIdx >= TopOfStack->colorAttachmentCount ||
        target != TopOfStack->colorAttachments.at(colorIdx)) {
      differentFromCurrent = true;
    }
    TopOfStack->colorAttachments.at(colorIdx) = target;
    TopOfStack->colorBlendEquations.at(colorIdx) = {
        .srcColorBlendFactor = target.blendMode.srcColorBlendFactor,
        .dstColorBlendFactor = target.blendMode.dstColorBlendFactor,
        .colorBlendOp = target.blendMode.colorBlendOp,
        .srcAlphaBlendFactor = target.blendMode.srcAlphaBlendFactor,
        .dstAlphaBlendFactor = target.blendMode.dstAlphaBlendFactor,
        .alphaBlendOp = target.blendMode.alphaBlendOp,
    };

    colorIdx++;
  }

  TopOfStack->colorAttachmentCount = colorIdx;
  SetViewport(nullptr);

  // Only clear if we have the same render targets as before
  // And have load ops that require clearing
  if (differentFromCurrent) {
    return Error::Success();
  }

  if (GetIsCurrentlyRendering()) {
    ClearInfo clearInfo{};
    for (int i = 0; i < TopOfStack->colorAttachmentCount; i++) {
      const auto &target = TopOfStack->colorAttachments.at(i);

      if (target.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        clearInfo.colors.emplace_back(target.clearValue.color.float32[0],
                                      target.clearValue.color.float32[1],
                                      target.clearValue.color.float32[2],
                                      target.clearValue.color.float32[3]);
      }
    }

    if (TopOfStack->hasDepthStencilAttachment &&
        TopOfStack->depthStencilAttachment.loadOp ==
            VK_ATTACHMENT_LOAD_OP_CLEAR) {
      clearInfo.clearDepth = true;
      clearInfo.depthClearValue =
          TopOfStack->depthStencilAttachment.clearValue.depthStencil.depth;
    }

    CHECK_ERR(Clear(context, clearInfo));
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

auto GetTargetSize() -> VkExtent2D {
  if (TopOfStack->colorAttachmentCount > 0) {
    return {
        TopOfStack->colorAttachments.at(0).texture->size.width,
        TopOfStack->colorAttachments.at(0).texture->size.height,
    };
  }

  if (TopOfStack->hasDepthStencilAttachment) {
    return {
        TopOfStack->depthStencilAttachment.texture->size.width,
        TopOfStack->depthStencilAttachment.texture->size.height,
    };
  }

  // No attachments, return zero size
  return {0, 0};
}

auto GetMaximumAllowedViewport() -> VkViewport {
  auto viewport = TopOfStack->viewport;

  auto size = GetTargetSize();

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
  VkExtent2D size = GetTargetSize();

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
  return {TopOfStack->colorAttachments.begin(),
          TopOfStack->colorAttachments.begin() + // NOLINT
              TopOfStack->colorAttachmentCount};
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

  if (!clearInfo.clearDepth && !clearInfo.clearStencil &&
      clearInfo.colors.empty()) {
    return {};
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

  for (uint32_t i = 0; i < TopOfStack->colorAttachmentCount; i++) {
    VkClearAttachment clearAttachment = {};
    const auto &rendertarget = TopOfStack->colorAttachments.at(i);

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

  if (clearInfo.clearDepth) {
    VkClearAttachment clearAttachment = {};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clearAttachment.clearValue.depthStencil.depth = clearInfo.depthClearValue;

    clearAttachments.emplace_back(clearAttachment);
    clearRects.emplace_back(clearRect);
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
    hash = StateKeyHash::Hash(StateKey(*this));
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
