#include "rendertarget.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/object.hpp"
#include "slang/slang.h"
#include "tl/expected.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>

namespace Graphics::RenderTarget {

auto GetSwapchainTextures() -> std::vector<Ref<Graphics::Texture::Texture>> & {
  static std::vector<Ref<Graphics::Texture::Texture>> textures = {};
  return textures;
}

auto inline BuildDescriptorSetLayoutBindings(
    const Shader::ShaderModule *shader, slang::TypeLayoutReflection *typeLayout,
    std::vector<VkPushConstantRange> &pushConstantRanges,
    std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
        &setBindingsMap) -> Error::Error {
  std::cout << "Building descriptor set layout bindings\n";
  std::cout << "Field count: " << typeLayout->getFieldCount() << "\n";

  for (uint32_t i = 0; i < typeLayout->getFieldCount(); ++i) {
    auto *fieldLayout = typeLayout->getFieldByIndex(i);

    uint32_t setIndex = fieldLayout->getBindingIndex();
    uint32_t binding = fieldLayout->getBindingSpace();

    if (binding == UINT32_MAX) {
      continue;
    }

    if (fieldLayout->getCategory() ==
        slang::ParameterCategory::PushConstantBuffer) {
      size_t size = fieldLayout->getTypeLayout()->getSize(
          slang::ParameterCategory::PushConstantBuffer);

      pushConstantRanges.push_back({
          .stageFlags = VK_SHADER_STAGE_ALL,
          .offset = uint32_t(fieldLayout->getOffset()),
          .size = uint32_t(size),
      });

      continue;
    }

    VkDescriptorSetLayoutBinding vkBinding{};
    vkBinding.binding = binding;
    vkBinding.descriptorCount = 1;
    vkBinding.stageFlags = VK_SHADER_STAGE_ALL;

    std::cout << fieldLayout->getCategory() << "\n";

    switch (fieldLayout->getCategory()) {
    case slang::ParameterCategory::ConstantBuffer:
      vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      break;
    case slang::ParameterCategory::ShaderResource: {
      switch (fieldLayout->getTypeLayout()->getKind()) {
      case slang::TypeReflection::Kind::TextureBuffer:
        vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        break;
      case slang::TypeReflection::Kind::Struct:
      case slang::TypeReflection::Kind::Array:
        // likely a buffer
        vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        break;
      default:
        vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        break;
      }
      break;
    }
    case slang::ParameterCategory::UnorderedAccess:
      vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      break;

    case slang::ParameterCategory::SamplerState:
      vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      break;
    case slang::ParameterCategory::DescriptorTableSlot: {
      // auto *tableLayout = fieldLayout->getTypeLayout();

      // auto error = BuildDescriptorSetLayoutBindings(
      //     shader, tableLayout, pushConstantRanges, setBindingsMap);

      continue;
    }
    default:
      std::cout << "Unsupported descriptor type in shader reflection\n";
      continue;
    }

    std::cout << "Set " << setIndex << ", binding " << binding << "\n";
    setBindingsMap[setIndex].push_back(vkBinding);
  }

  std::cout << "Processing descriptor sets\n";
  std::cout << "Descriptor set count: " << typeLayout->getDescriptorSetCount()
            << "\n";

  for (uint32_t i = 0; i < typeLayout->getDescriptorSetCount(); ++i) {
    auto count = typeLayout->getDescriptorSetDescriptorRangeCount(i);
    std::cout << "Descriptor set " << i << " has " << count << " bindings\n";

    for (uint32_t j = 0; j < count; ++j) {
      auto arraySize =
          typeLayout->getDescriptorSetDescriptorRangeDescriptorCount(i, j);
      auto type = typeLayout->getDescriptorSetDescriptorRangeType(i, j);
      auto category = typeLayout->getDescriptorSetDescriptorRangeCategory(i, j);
      auto index = typeLayout->getDescriptorSetDescriptorRangeIndexOffset(i, j);

      VkDescriptorSetLayoutBinding vkBinding{};
      vkBinding.binding = index;
      vkBinding.descriptorCount = arraySize;
      vkBinding.stageFlags = VK_SHADER_STAGE_ALL;

      switch (category) {
      case slang::ParameterCategory::ConstantBuffer:
        vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;
      case slang::ParameterCategory::ShaderResource: {
        switch (type) {
        case slang::BindingType::Texture:
          vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
          break;
        case slang::BindingType::RawBuffer:
        case slang::BindingType::TypedBuffer:
          vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          break;
        case slang::BindingType::Unknown:
          break;
        case slang::BindingType::Sampler:
          vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
          break;
        case slang::BindingType::ConstantBuffer:
          vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          break;
        case slang::BindingType::ParameterBlock:
          // Handled separately
          break;
        case slang::BindingType::CombinedTextureSampler:
          vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
          break;
        case slang::BindingType::InputRenderTarget:
          vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
          break;
        case slang::BindingType::InlineUniformData:
          // Not supported in Vulkan
          break;
        case slang::BindingType::RayTracingAccelerationStructure:
          vkBinding.descriptorType =
              VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
          break;
        case slang::BindingType::VaryingInput:
        case slang::BindingType::VaryingOutput:
        case slang::BindingType::ExistentialValue:
        case slang::BindingType::PushConstant:
        case slang::BindingType::MutableFlag:
        case slang::BindingType::MutableTexture:
        case slang::BindingType::MutableTypedBuffer:
        case slang::BindingType::MutableRawBuffer:
        case slang::BindingType::BaseMask:
        case slang::BindingType::ExtMask:
          std::cout << "Unsupported shader resource type in reflection\n";
          std::cout << "Type: " << static_cast<int>(type) << "\n";
          break;
        }
        break;
      }

      case slang::None:
      case slang::Mixed:
      case slang::UnorderedAccess:
      case slang::VaryingInput:
      case slang::VaryingOutput:
      case slang::SamplerState:
      case slang::Uniform:
      case slang::DescriptorTableSlot:
      case slang::SpecializationConstant:
      case slang::PushConstantBuffer:
      case slang::RegisterSpace:
      case slang::GenericResource:
      case slang::RayPayload:
      case slang::HitAttributes:
      case slang::CallablePayload:
      case slang::ShaderRecord:
      case slang::ExistentialTypeParam:
      case slang::ExistentialObjectParam:
      case slang::SubElementRegisterSpace:
      case slang::InputAttachmentIndex:
      case slang::MetalArgumentBufferElement:
      case slang::MetalAttribute:
      case slang::MetalPayload:
        std::cout << "Unsupported descriptor type in shader reflection\n";
        std::cout << "Category: " << static_cast<int>(category) << "\n";
        break;
      }

      setBindingsMap[i].push_back(vkBinding);
    }
  }

  return Error::Success();
}

auto GetPipelineLayout(const GraphicsContext &context,
                       const Shader::ShaderModule *shader)
    -> tl::expected<VkPipelineLayout, Error::Error> {
  auto *globalLayout = shader->programLayout->getGlobalParamsVarLayout();

  std::cout << globalLayout << "\n";

  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      setBindingsMap;

  auto *typeLayout = globalLayout->getTypeLayout();

  auto pushConstantRanges = std::vector<VkPushConstantRange>{};

  std::cout << "Creating pipeline layout from shader reflection\n";

  std::vector<VkDescriptorSetLayout> setLayouts;

  auto error = BuildDescriptorSetLayoutBindings(
      shader, typeLayout, pushConstantRanges, setBindingsMap);
  if (Error::IsError(error)) {
    return tl::make_unexpected(error);
  }

  std::cout << "Creating descriptor set layouts\n";

  for (const auto &setBinding : setBindingsMap) {
    uint32_t setIndex = setBinding.first;
    const auto &bindings = setBinding.second;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    auto error = Error::Create(vkCreateDescriptorSetLayout(
        context.device, &layoutInfo, nullptr, &descriptorSetLayout));

    if (Error::IsError(error)) {
      return tl::make_unexpected(error);
    }

    setLayouts.emplace_back(descriptorSetLayout);
  }

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  error = Error::Create(vkCreatePipelineLayout(
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
  swapchainRendertarget->blendMode = {};
  swapchainRendertarget->clearValue = {0.0F, 0.0F, 0.0F, 1.0F};
  return swapchainRendertarget;
}

inline auto CreatePipeline(const GraphicsContext &context, const State &state)
    -> tl::expected<VkPipeline, Error::Error> {
  if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return tl::make_unexpected(
        Error::Create("Only graphics pipelines are supported currently"));
  }

  std::cout << "Creating graphics pipeline\n";

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

  auto *shader = state.shader.get();

  if (shader == nullptr) {
    shader = Shader::DefaultShaderModule.get();
  }

  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = shader->module;
  shaderStages[0].pName = "vertexMain";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = shader->module;
  shaderStages[1].pName = "fragmentMain";

  std::cout << "Setting up vertex input state\n";

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

  std::cout << "Setting up input assembly state\n";

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

  std::cout << "Determining expected output attachments\n";

  auto entryPointIndex =
      shader->entryPointToStageIndex.at(SlangStage::SLANG_STAGE_FRAGMENT);
  std::cout << "Fragment entry point index: " << entryPointIndex << "\n";

  std::cout << shader->programLayout << "\n";
  std::cout << shader->programLayout->getEntryPointByIndex(1) << "\n";
  std::cout << shader->programLayout->getEntryPointByIndex(1)->getName()
            << "\n";

  auto *entryPoint =
      shader->programLayout->getEntryPointByIndex(entryPointIndex);

  if (entryPoint == nullptr) {
    return tl::make_unexpected(Error::Create(
        "Failed to get fragment entry point from shader program layout"));
  }

  std::cout << "Fetched entry point reflection.\n";
  auto *outputVariableLayout = entryPoint->getResultVarLayout();

  if (outputVariableLayout == nullptr) {
    return tl::make_unexpected(Error::Create(
        "Shader has no output variable layout for fragment stage"));
  }

  std::cout << "Determining expected output attachments\n";

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

  std::cout << "Expected attachments:\n";
  for (const auto &att : expectedAttachments) {
    std::cout << " - " << att << "\n";
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

  std::cout << "Getting pipeline layout\n";

  auto layoutResult = GetPipelineLayout(context, shader);
  if (Error::IsError(layoutResult)) {
    return tl::make_unexpected(layoutResult.error());
  }

  pipelineInfo.layout = layoutResult.value();
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &renderingCreateInfo;

  VkPipeline pipeline = VK_NULL_HANDLE;

  auto error = Error::Create(vkCreateGraphicsPipelines(
      context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

  std::cout << "STORING PIPELINE IN CACHE\n";
  PipelineCache[state] = pipeline;

  if (Error::IsError(error)) {
    return tl::make_unexpected(error);
  }

  return pipeline;
}

inline auto GetPipeline(const GraphicsContext &context, const State &state)
    -> tl::expected<VkPipeline, Error::Error> {

  std::cout << "Getting pipeline from cache\n";

  auto cacheIterator = PipelineCache.find(state);

  std::cout << "Cache iterator found\n";

  if (cacheIterator != PipelineCache.end()) {
    std::cout << "Pipeline found in cache\n";

    return cacheIterator->second;
  }

  auto result = CreatePipeline(context, state);

  if (Error::IsError(result)) {
    return result;
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
      .width = static_cast<float>(context.swapchainInfo.extent.width),
      .height = static_cast<float>(context.swapchainInfo.extent.height),
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };

  defaultState.scissor = {
      .offset = {0, 0},
      .extent = {context.swapchainInfo.extent.width,
                 context.swapchainInfo.extent.height},
  };

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

  std::cout << "Flushing render target state changes\n";
  auto pipelineResult = GetPipeline(context, currentState);
  if (Error::IsError(pipelineResult)) {
    return tl::make_unexpected(pipelineResult.error());
  }

  const auto &commandBuffer =
      Graphics::GetCommandBuffer(context, GetCurrentThreadIndex());

  vkCmdBindPipeline(commandBuffer, currentState.bindPoint,
                    pipelineResult.value());

  return true;
}

auto Destroy(GraphicsContext &context) -> void {
  for (const auto &pair : PipelineCache) {
    vkDestroyPipeline(context.device, pair.second, nullptr);
  }
  PipelineCache.clear();
}

// NOLINTNEXTLINE, to call vkCmdEndRendering
static bool BegunRendering = false;

inline auto BeginRendering(GraphicsContext &context) -> void {
  auto &currentState = StateStack.back();

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.offset = {
      .x = static_cast<int32_t>(currentState.viewport.x),
      .y = static_cast<int32_t>(currentState.viewport.y)};
  renderingInfo.renderArea.extent = {
      .width = static_cast<uint32_t>(currentState.viewport.width),
      .height = static_cast<uint32_t>(currentState.viewport.height)};
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  auto colorAttachments = std::vector<VkRenderingAttachmentInfo>{};
  auto depthAttachment = VkRenderingAttachmentInfo{};
  auto stencilAttachment = VkRenderingAttachmentInfo{};

  for (const auto &rendertarget : currentState.renderTargets) {
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
    EndRendering(context);
    std::cout << "Beginning rendering\n";
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

// TODO: Remove shader handles and use Ref<ShaderModule> instead
auto SetShader(const Ref<Shader::ShaderModule> &shader) -> void {
  StateStack.back().shader = shader;
}

auto SetRenderTargets(const std::vector<Ref<RenderTarget>> &renderTargets)
    -> void {
  StateStack.back().renderTargets = renderTargets;
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

auto GetScissor() -> VkRect2D {
  auto &currentState = StateStack.back();
  return currentState.scissor;
}

auto GetShader() -> Ref<Shader::ShaderModule> {
  auto &currentState = StateStack.back();
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
