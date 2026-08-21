#include "commands.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/renderState.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/image.hpp"
#include "dynamicRendering.hpp"
#include <vector>

namespace Graphics {

std::unordered_map<GraphState, uint32_t, GraphStateHash>
    CommandStateManager::StateToIndex{};
std::vector<GraphState> CommandStateManager::States{};

auto DrawState::GetStateFor(void const *resource) const
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
  VkAccessFlags2 access = 0;
  VkPipelineStageFlags2 pipelines = 0;

  for (const auto &attachment : colorAttachments) {
    if (attachment == resource) {
      access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      pipelines |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
  }

  if (depthStencilAttachment == resource) {
    access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    pipelines |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                 VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  }

  for (const auto &bound : boundImages) {
    if (bound.image == resource) {
      access |= bound.access;
      pipelines |= bound.pipelines;
    }
  }

  for (const auto &bound : boundBuffers) {
    if (bound.buffer == resource) {
      access |= bound.access;
      pipelines |= bound.pipelines;
    }
  }

  for (const auto &buffer : vertexBuffers) {
    if (buffer == resource) {
      access |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
      pipelines |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    }
  }

  if (indexBuffer == resource) {
    access |= VK_ACCESS_2_INDEX_READ_BIT;
    pipelines |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }

  return {access, pipelines};
}

auto GraphState::GetHash() const -> uint64_t {
  if (dirty) {
    Hash::Hasher hasher{};

    // Special case for compute pipelines
    if (bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      hasher.Add(std::hash<VkPipelineBindPoint>()(bindPoint));
      hasher.Add(shader->getID());
      return hasher.Get();
    }

    hasher.Add(cullMode);
    hasher.Add(frontFace);
    hasher.Add(depthTestEnable);
    hasher.Add(depthWriteEnable);
    hasher.Add(depthCompareOp);
    hasher.Add(stencilTestEnable);
    hasher.Add(polygonMode);
    hasher.Add(viewport.x);
    hasher.Add(viewport.y);
    hasher.Add(viewport.width);
    hasher.Add(viewport.height);
    hasher.Add(scissor.extent.width);
    hasher.Add(scissor.extent.height);
    hasher.Add(scissor.offset.x);
    hasher.Add(scissor.offset.y);

    for (auto *buffer : vertexBuffers) {
      hasher.Add(buffer);
    }

    hasher.Add(indexBuffer);
    hasher.Add(indexType);

    for (auto offset : vertexBufferOffsets) {
      hasher.Add(offset);
    }
    hasher.Add(indexBufferOffset);

    for (const auto &equation : colorBlendEquations) {
      hasher.Add(equation.srcColorBlendFactor);
      hasher.Add(equation.dstColorBlendFactor);
      hasher.Add(equation.colorBlendOp);
      hasher.Add(equation.srcAlphaBlendFactor);
      hasher.Add(equation.dstAlphaBlendFactor);
      hasher.Add(equation.alphaBlendOp);
    }

    if (shader) {
      hasher.Add(shader->getID());
    }

    hasher.Add(primitiveTopology);
    hasher.Add(bindPoint);

    for (const auto &attachment : colorAttachments) {
      hasher.Add(attachment.texture->view);
    }

    if (hasDepthStencilAttachment) {
      hasher.Add(depthStencilAttachment.texture->view);
    }

    for (auto enable : blendEnables) {
      hasher.Add(enable);
    }

    for (auto mask : colorWriteMasks) {
      hasher.Add(mask);
    }

    for (const auto &desc : bindingDescriptions) {
      hasher.Add(desc.binding);
      hasher.Add(desc.stride);
      hasher.Add(desc.inputRate);
    }

    for (const auto &desc : attributeDescriptions) {
      hasher.Add(desc.location);
      hasher.Add(desc.binding);
      hasher.Add(desc.format);
      hasher.Add(desc.offset);
    }

    hash = hasher.Get();

    dirty = false;
  }

  return hash;
}

DrawState::DrawState() {
  const auto &shader = RenderState::GetShader();

  bool isCompute =
      (shader->combinedShaderStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;

  if (!isCompute) {
    const auto &rendertargets = RenderState::GetRenderTargets();

    for (const auto &target : rendertargets) {
      if (Image::IsDepthOrStencilTexture(target.texture->GetFormat())) {
        depthStencilAttachment = target.texture->imageMemory->image;
      } else {
        colorAttachments.emplace_back(target.texture->imageMemory->image);
      }
    }
  }

  const auto &shaderState = shader->GetState();
  const auto &pipelines = shader->combinedPipelineStages;

  // TODO: Distinguish between vertex / fragment accesses
  for (const auto &buffer : shaderState.userBoundBuffers) {
    boundBuffers.emplace_back(BoundBuffer{
        .buffer = buffer.second.first->handle,
        .access = buffer.second.second->accessFlags,
        .pipelines = pipelines,
    });
  }

  for (const auto &texture : shaderState.userBoundTextures) {
    boundImages.emplace_back(BoundImage{
        .image = texture.second.first->imageMemory->image,
        .access = texture.second.second->accessFlags,
        .pipelines = pipelines,
    });
  }

  for (const auto &accelerationStructure :
       shaderState.userBoundAccelerationStructures) {
    boundAccelerationStructures.emplace_back(
        accelerationStructure.second.first->GetAccelerationStructure());
  }

  const auto &ctx = GetThreadContext();

  vertexBuffers.insert(vertexBuffers.begin(), ctx.boundVertexBuffers.begin(),
                       ctx.boundVertexBuffers.end());
  indexBuffer = ctx.boundIndexBuffer;

  stateID = ctx.commandBuffer->GetStateID();
  pushConstants = ctx.commandBuffer->GetGraphState().pushConstants;
}

auto GetReads(const Command &command) -> std::vector<void *> {
  const auto *bound = get_if_derived<BoundResources>(command.data);

  if (bound != nullptr) {
    return bound->reads;
  }

  return {};
}

auto GetWrites(const Command &command) -> std::vector<void *> {
  const auto *bound = get_if_derived<BoundResources>(command.data);

  if (bound != nullptr) {
    return bound->writes;
  }

  return {};
}

auto VirtualCommandBuffer::AddCommand(const Command &command) -> void {
  commands.emplace_back(command);
  time++;
}

auto VirtualCommandBuffer::Draw(const Args::VkCmdDraw &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::DrawIndexed(const Args::VkCmdDrawIndexed &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::DrawIndirect(
    const Args::VkCmdDrawIndirect &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::DrawIndexedIndirect(
    const Args::VkCmdDrawIndexedIndirect &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::Dispatch(const Args::VkCmdDispatch &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::DispatchIndirect(
    const Args::VkCmdDispatchIndirect &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BlitImage(const Args::VkCmdBlitImage &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::PushConstants(
    const Args::VkCmdPushConstants &arguments) -> void {
  currentState.pushConstants = arguments.values;
}

auto VirtualCommandBuffer::CopyBuffer(const Args::VkCmdCopyBuffer &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyImage(const Args::VkCmdCopyImage &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyBufferToImage(
    const Args::VkCmdCopyBufferToImage &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyImageToBuffer(
    const Args::VkCmdCopyImageToBuffer &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::MipmapTexture(const Args::MipmapTexture &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::FillBuffer(const Args::VkCmdFillBuffer &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BuildAccelerationStructuresKHR(
    const Args::VkCmdBuildAccelerationStructuresKHR &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyAccelerationStructureKHR(
    const Args::VkCmdCopyAccelerationStructureKHR &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::ResetQueryPool(
    const Args::VkCmdResetQueryPool &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::WriteAccelerationStructuresPropertiesKHR(
    const Args::VkCmdWriteAccelerationStructuresPropertiesKHR &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BindIndexBuffer(
    const Args::VkCmdBindIndexBuffer &arguments) -> void {
  currentState.indexBuffer = arguments.buffer;
  currentState.indexType = arguments.indexType;
  currentState.indexBufferOffset = arguments.offset;
}

auto VirtualCommandBuffer::BindVertexBuffers(
    const Args::VkCmdBindVertexBuffers &arguments) -> void {

  const auto first = arguments.firstBinding;
  const auto count = arguments.buffers.size();
  currentState.vertexBuffers.resize(first + count);
  currentState.vertexBufferOffsets.resize(first + count);

  for (size_t i = 0; i < count; ++i) {
    currentState.vertexBuffers[first + i] = arguments.buffers[i];
    currentState.vertexBufferOffsets[first + i] = arguments.offsets[i];
  }
}

auto VirtualCommandBuffer::SetVertexInputEXT(
    const Args::VkCmdSetVertexInputEXT &arguments) -> void {
  currentState.bindingDescriptions = arguments.bindingDescriptions;
  currentState.attributeDescriptions = arguments.attributeDescriptions;
}

auto VirtualCommandBuffer::BindPipeline(
    const Args::VkCmdBindPipeline &arguments) -> void {}

auto VirtualCommandBuffer::BindDescriptorSets(
    const Args::VkCmdBindDescriptorSets &arguments) -> void {}

auto VirtualCommandBuffer::SetViewport(const Args::VkCmdSetViewport &arguments)
    -> void {
  currentState.viewport = arguments.viewports.front();
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetScissor(const Args::VkCmdSetScissor &arguments)
    -> void {
  currentState.scissor = arguments.scissors.front();
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetDepthTestEnable(
    const Args::VkCmdSetDepthTestEnable &arguments) -> void {
  currentState.depthTestEnable = arguments.depthTestEnable;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetDepthWriteEnable(
    const Args::VkCmdSetDepthWriteEnable &arguments) -> void {
  currentState.depthWriteEnable = arguments.depthWriteEnable;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetDepthCompareOp(
    const Args::VkCmdSetDepthCompareOp &arguments) -> void {
  currentState.depthCompareOp = arguments.depthCompareOp;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetColorBlendEquationEXT(
    const Args::VkCmdSetColorBlendEquationEXT &arguments) -> void {
  currentState.colorBlendEquations = arguments.equations;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetColorBlendEnableEXT(
    const Args::VkCmdSetColorBlendEnableEXT &arguments) -> void {
  currentState.blendEnables = arguments.enables;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetColorWriteMaskEXT(
    const Args::VkCmdSetColorWriteMaskEXT &arguments) -> void {
  currentState.colorWriteMasks = arguments.masks;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetCullMode(const Args::VkCmdSetCullMode &arguments)
    -> void {
  currentState.cullMode = arguments.cullMode;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetFrontFace(
    const Args::VkCmdSetFrontFace &arguments) -> void {
  currentState.frontFace = arguments.frontFace;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::ClearAttachments(
    const Args::VkCmdClearAttachments &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BeginDebugUtilsLabelEXT(
    const Args::VkCmdBeginDebugUtilsLabelEXT &arguments) -> void {
  // currentState.currentDebugMarker = arguments.labelInfo.pLabelName;
}

auto VirtualCommandBuffer::EndDebugUtilsLabelEXT(
    const Args::VkCmdEndDebugUtilsLabelEXT &arguments) -> void {}

auto VirtualCommandBuffer::InsertDebugUtilsLabelEXT(
    const Args::VkCmdInsertDebugUtilsLabelEXT &arguments) -> void {}

auto CreateCommandBuffer() -> VirtualCommandBuffer { return {}; }

auto VirtualCommandBuffer::Reset() -> void {
  time = 0;
  commands.clear();
  queueFamily = UINT32_MAX;
}

auto ResetCommandBuffer(VirtualCommandBuffer &buffer) -> void {
  buffer.Reset();
}

auto DrawState::Apply(const GraphicsContext &context,
                      VirtualCommandBuffer &buffer,
                      VkCommandBuffer cmdBuffer) const -> Error {
  const auto &state = CommandStateManager::States.at(stateID);

  CHECK_ERR(PrepareRendering(context, cmdBuffer));

  return {};
}

} // namespace Graphics