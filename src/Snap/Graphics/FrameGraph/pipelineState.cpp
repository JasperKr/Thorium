#include "Graphics/FrameGraph/pipelineState.hpp"
#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/uniformWriter.hpp"
#include <mutex>

namespace Graphics {

inline auto GetShaderStages(const RenderState::State &state)
    -> Result<std::vector<VkPipelineShaderStageCreateInfo>> {
  ZoneScoped;

  thread_local std::unordered_map<Shader *,
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

  for (const auto &stage : shader->entryPoints) {
    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage.second;
    stageInfo.module = shader->module;
    stageInfo.pName = stage.first.c_str();

    shaderStages.emplace_back(stageInfo);
  }

  shaderStageCache[state.shader.get()] = shaderStages;

  return shaderStages;
}

auto inline GetInputAssemblyState(const RenderState::State &state)
    -> VkPipelineInputAssemblyStateCreateInfo {
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = state.primitiveTopology;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  return inputAssembly;
}

auto inline GetRasterizationState(const RenderState::State &state)
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
                                         const RenderState::State &state)
    -> Result<std::vector<VkPipelineColorBlendAttachmentState>> {

  // Get Shader Output Reflection //

  auto *shader = state.shader.get();

  // #ifndef NDEBUG
  //   if (!shader->entryPointToStageIndex.contains(
  //           SlangStage::SLANG_STAGE_FRAGMENT)) {
  //     return std::vector<VkPipelineColorBlendAttachmentState>{};
  //   }

  //   auto entryPointIndex =
  //       shader->entryPointToStageIndex.at(SlangStage::SLANG_STAGE_FRAGMENT);

  //   auto *entryPoint =
  //       shader->programLayout->getEntryPointByIndex(entryPointIndex);

  //   if (entryPoint == nullptr) {
  //     return Error::Unexpected(
  //         "Failed to get fragment entry point from shader program layout");
  //   }

  //   auto *outputVariableLayout = entryPoint->getResultVarLayout();

  //   if (outputVariableLayout == nullptr) {
  //     // No fragment outputs, so no blend attachments needed
  //     return std::vector<VkPipelineColorBlendAttachmentState>{};
  //   }

  //   // Determine Expected Output Attachments //

  //   std::unordered_set<uint32_t> expectedAttachments = {};
  //   for (uint32_t i = 0;
  //        i < outputVariableLayout->getTypeLayout()->getFieldCount(); ++i) {
  //     auto *outVar = outputVariableLayout->getTypeLayout()->getFieldByIndex(i);
  //     if (outVar->getSemanticName() != nullptr &&
  //         strcmp(outVar->getSemanticName(), "SV_Target") == 0) {
  //       expectedAttachments.insert(outVar->getSemanticIndex());
  //     }
  //   }
  // #endif

  // Get Actual Set Render Targets //

  auto blendAttachments = std::vector<VkPipelineColorBlendAttachmentState>();
  blendAttachments.resize(MAX_COLOR_ATTACHMENTS);

  bool hasImplicitLocation = false;
  bool hasExplicitLocation = false;

  for (int i = 0; i < state.colorAttachments.size(); ++i) {
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

  // #ifndef NDEBUG
  //   for (uint32_t i = 0; i <= blendAttachments.size(); ++i) {
  //     if (expectedAttachments.contains(i)) {
  //       if (i >= blendAttachments.size()) {
  //         return Error::Unexpected(
  //             "Missing blend attachment for expected output location " +
  //             std::to_string(i));
  //       }
  //     }
  //   }
  // #endif

  return blendAttachments;
}

auto GetTargetSize() -> VkExtent2D {
  if (CurrentState.colorAttachments.size() > 0) {
    return {
        CurrentState.colorAttachments.at(0).texture->GetWidth(),
        CurrentState.colorAttachments.at(0).texture->GetHeight(),
    };
  }

  if (CurrentState.hasDepthStencilAttachment) {
    return {
        CurrentState.depthStencilAttachment.texture->GetWidth(),
        CurrentState.depthStencilAttachment.texture->GetHeight(),
    };
  }

  // No attachments, return zero size
  return {0, 0};
}

auto GetMaximumAllowedViewport() -> VkViewport {
  auto viewport = CurrentState.viewport;

  auto size = GetTargetSize();

  viewport.width = static_cast<float>(size.width);
  viewport.height = static_cast<float>(size.height);

  viewport.width = (std::max)(viewport.width, 1.0F);
  viewport.height = (std::max)(viewport.height, 1.0F);

  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  return viewport;
}

auto GetScissor() -> VkRect2D {

  if (!CurrentState.hasScissor) {
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

  return {
      .offset =
          {
              .x = CurrentState.scissor.offset.x,
              .y = CurrentState.scissor.offset.y,
          },
      .extent =
          {
              .width = (std::max)(CurrentState.scissor.extent.width, 1U),
              .height = (std::max)(CurrentState.scissor.extent.height, 1U),
          },
  };
}

auto GetClippedViewport() -> VkViewport {

  if (!CurrentState.hasViewport) {
    return GetMaximumAllowedViewport();
  }

  auto viewport = CurrentState.viewport;

  // Default to size of current attachments
  VkExtent2D size = GetTargetSize();

  viewport.width = std::min(viewport.width, static_cast<float>(size.width));
  viewport.height = std::min(viewport.height, static_cast<float>(size.height));

  return viewport;
}

auto GetViewport() -> VkViewport {

  if (!CurrentState.hasViewport) {
    return GetMaximumAllowedViewport();
  }

  // return CurrentState.viewport;
  return {
      .x = CurrentState.viewport.x,
      .y = CurrentState.viewport.y,
      .width = (std::max)(CurrentState.viewport.width, 1.0F),
      .height = (std::max)(CurrentState.viewport.height, 1.0F),
      .minDepth = CurrentState.viewport.minDepth,
      .maxDepth = CurrentState.viewport.maxDepth,
  };
}

inline auto CreateGraphicsPipeline(const GraphicsContext &context,
                                   RenderState::State &state)
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
      depthFormat = rendertarget.texture->GetFormat();
    }
    if (rendertarget.texture->IsStencilTexture()) {
      stencilFormat = rendertarget.texture->GetFormat();
    }
  }

  bool hasImplicitLocation = false;
  bool hasExplicitLocation = false;

  for (int i = 0; i < state.colorAttachments.size(); i++) {
    const auto &rendertarget = state.colorAttachments.at(i);

    int location = rendertarget.location;
    if (location == -1) {
      location = i;
      hasImplicitLocation = true;
    } else {
      hasExplicitLocation = true;
    }

    formats.at(location) = rendertarget.texture->GetFormat();
  }

  if (hasImplicitLocation && hasExplicitLocation) {
    return Error::Unexpected(
        "Cannot mix explicit and implicit render target locations.");
  }

  /// Color Attachments ///

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = state.colorAttachments.size();
  renderingCreateInfo.pColorAttachmentFormats = formats.data();
  renderingCreateInfo.depthAttachmentFormat = depthFormat;
  renderingCreateInfo.stencilAttachmentFormat = stencilFormat;

  auto blendAttachments =
      CHECK_RES(GetColorBlendAttachmentState(context, state));

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = state.colorAttachments.size();
  colorBlending.pAttachments = blendAttachments.data();

  if (state.colorAttachments.size() == 0) {
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
  auto layout =
      CHECK_RES(GetPipelineCache().GetPipelineLayout(context, shader));

  pipelineInfo.layout = layout.layout;
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &renderingCreateInfo;

  PrintDebug("Creating graphics pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateGraphicsPipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
        GetAllocationCallbacks(), &pipeline));
  }

  {
    std::lock_guard<std::mutex> lock(GetPipelineCache().mutex);

    auto key = StateKey(state);
    GetPipelineCache().cache.emplace(key, std::make_pair(pipeline, layout));
    GetPipelineCache().pipelines.emplace_back(pipeline);
  }

  return std::pair<VkPipeline, PipelineLayout>(pipeline, layout);
}

inline auto CreateComputePipeline(const GraphicsContext &context,
                                  RenderState::State &state)
    -> Result<std::pair<VkPipeline, PipelineLayout>> {
  ZoneScoped;

  VkComputePipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelineInfo.stage.module = state.shader->module;

  if ((state.shader->combinedShaderStages & VK_SHADER_STAGE_COMPUTE_BIT) == 0) {
    return Error::Unexpectedf(
        "Shader module {} does not have a compute shader stage.",
        state.shader->name);
  }

  // If we get a validation error / crash about this not existing; rename to "main"
  pipelineInfo.stage.pName = "computeMain";

  auto layout = CHECK_RES(
      GetPipelineCache().GetPipelineLayout(context, state.shader.get()));
  pipelineInfo.layout = layout.layout;

  PrintDebug("Creating compute pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateComputePipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
        GetAllocationCallbacks(), &pipeline));
  }

  {
    std::lock_guard<std::mutex> lock(GetPipelineCache().mutex);

    auto key = StateKey(state);
    GetPipelineCache().cache.emplace(key, std::make_pair(pipeline, layout));
    GetPipelineCache().pipelines.emplace_back(pipeline);
  }

  return std::pair<VkPipeline, PipelineLayout>(pipeline, layout);
}

inline auto CreatePipeline(const GraphicsContext &context,
                           RenderState::State &state)
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

inline auto GetPipeline(const GraphicsContext &context,
                        RenderState::State &state)
    -> Result<std::pair<VkPipeline, PipelineLayout>> {
  ZoneScoped;

  {
    std::lock_guard<std::mutex> lock(GetPipelineCache().mutex);
    auto *pipeline = GetPipelineCache().cache.get(StateKey(state));

    if (pipeline != nullptr) {
      return *pipeline;
    }
  }

  return CreatePipeline(context, state);
}

auto FlushCompute(const GraphicsContext &context,
                  VkCommandBuffer vkCommandBuffer) -> Result<bool> {
  ZoneScoped;

  if (CurrentState.bindPoint != VK_PIPELINE_BIND_POINT_COMPUTE) {
    return Error::Unexpected("Current state is not a compute pipeline.");
  }

  auto pipeline = CHECK_RES(GetPipeline(context, CurrentState));

  auto *commandBuffer = Graphics::GetVirtualCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Unexpected("Command buffer is null in FlushCompute.");
  }

  assert(CurrentState.shader->entryPoints.at(0).second ==
         VK_SHADER_STAGE_COMPUTE_BIT);

  GetPipelineCache().currentLayout = pipeline.second;

  PrintDebug("Binding pipeline");

  vkCmdBindPipeline(vkCommandBuffer, CurrentState.bindPoint, pipeline.first);

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

auto FlushGraphics(const GraphicsContext &context,
                   VkCommandBuffer vkCommandBuffer) -> Result<bool> {
  ZoneScoped;

  ERR_ASSERT(CurrentState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS);

  auto pipeline = CHECK_RES(GetPipeline(context, CurrentState));

  auto *commandBuffer = CHECK_NULL(Graphics::GetVirtualCommandBuffer());

  auto viewport = GetClippedViewport();

  auto translationMatrix = Math::Matrix4x4::TranslationMatrix(
      {-viewport.width / 2.0F, -viewport.height / 2.0F, 0.0F}); // NOLINT

  Math::Matrix4x4 projectionMatrix = Math::Matrix4x4::Orthographic(
      viewport.width, viewport.height, 0.0F, 1.0F);

  auto viewProjectionMatrix = translationMatrix * projectionMatrix;

  static auto projectionMatrixKey =
      ResourceKey{"PushConstants", "DefaultProjectionMatrix"};

  if (CurrentState.shader->GetUniform(projectionMatrixKey) != nullptr) {
    CHECK_ERR(UniformWriter::Send(CurrentState.shader, projectionMatrixKey,
                                  viewProjectionMatrix));
  }

  GetPipelineCache().currentLayout = pipeline.second;

  PrintDebug("Binding pipeline");

  {
    ZoneScopedN("vkCmdBindPipeline");
    vkCmdBindPipeline(vkCommandBuffer, CurrentState.bindPoint, pipeline.first);
  }

  return true;
}

auto Flush(const GraphicsContext &context, VkCommandBuffer vkCommandBuffer)
    -> Result<bool> {
  ZoneScoped;

  [[likely]]
  if (!Graphics::GetIsStateDirty() && LastState != nullptr &&
      CurrentState.GetHash() == LastState->GetHash() &&
      CurrentState == *LastState) {
    return false;
  }

  LastStateStorage = CurrentState; // Copy current state to last state storage
  LastState = &LastStateStorage;   // Point last state to the storage

  if (CurrentState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    auto result = FlushGraphics(context, vkCommandBuffer);
    Graphics::GetIsStateDirty() = false;

    return result;
  }
  if (CurrentState.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    auto result = FlushCompute(context, vkCommandBuffer);
    Graphics::GetIsStateDirty() = false;

    return result;
  }

  return Error::Unexpected("Unsupported pipeline bind point in Flush.");
}

} // namespace Graphics