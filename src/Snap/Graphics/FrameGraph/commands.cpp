#include "commands.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Modules/image.hpp"

namespace Graphics {

Args::BoundResources::BoundResources() {
  const auto &shader = DynamicRendering::GetShader();

  bool isCompute =
      (shader->combinedShaderStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;

  if (!isCompute) {
    const auto &rendertargets = DynamicRendering::GetRenderTargets();

    for (const auto &target : rendertargets) {
      if (Image::IsDepthOrStencilTexture(target.texture->GetFormat())) {
        depthStencilAttachment = target.texture->view;
      } else {
        colorAttachments.emplace_back(target.texture->view);
      }
    }
  }

  const auto &shaderState = shader->GetState();

  for (const auto &buffer : shaderState.userBoundBuffers) {
    boundBuffers.emplace_back(buffer.second.first->handle);
  }

  for (const auto &texture : shaderState.userBoundTextures) {
    boundImages.emplace_back(texture.second.first->view);
  }
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
  AddCommand(Command(arguments));
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

auto VirtualCommandBuffer::PipelineBarrier2(
    const Args::VkCmdPipelineBarrier2 &arguments) -> void {
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
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BindVertexBuffers(
    const Args::VkCmdBindVertexBuffers &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetVertexInputEXT(
    const Args::VkCmdSetVertexInputEXT &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BindPipeline(
    const Args::VkCmdBindPipeline &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BeginRendering(
    const Args::VkCmdBeginRendering &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::EndRendering(
    const Args::VkCmdEndRendering &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BindDescriptorSets(
    const Args::VkCmdBindDescriptorSets &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetViewport(const Args::VkCmdSetViewport &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetScissor(const Args::VkCmdSetScissor &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetDepthTestEnable(
    const Args::VkCmdSetDepthTestEnable &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetDepthWriteEnable(
    const Args::VkCmdSetDepthWriteEnable &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetDepthCompareOp(
    const Args::VkCmdSetDepthCompareOp &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetColorBlendEquationEXT(
    const Args::VkCmdSetColorBlendEquationEXT &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetColorBlendEnableEXT(
    const Args::VkCmdSetColorBlendEnableEXT &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetColorWriteMaskEXT(
    const Args::VkCmdSetColorWriteMaskEXT &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetCullMode(const Args::VkCmdSetCullMode &arguments)
    -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::SetFrontFace(
    const Args::VkCmdSetFrontFace &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::ClearAttachments(
    const Args::VkCmdClearAttachments &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BeginDebugUtilsLabelEXT(
    const Args::VkCmdBeginDebugUtilsLabelEXT &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::EndDebugUtilsLabelEXT(
    const Args::VkCmdEndDebugUtilsLabelEXT &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::InsertDebugUtilsLabelEXT(
    const Args::VkCmdInsertDebugUtilsLabelEXT &arguments) -> void {
  AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::AddStateUpdate(VkBuffer buffer,
                                          const ResourceState &newState)
    -> void {
  bufferStateUpdates[buffer].emplace_back(newState, time);
  bufferStateUpdateTimeline.emplace_back(buffer, newState, time);
}

auto VirtualCommandBuffer::AddStateUpdate(VkImageView image,
                                          const ResourceState &newState)
    -> void {
  imageStateUpdates[image].emplace_back(newState, time);
  imageStateUpdateTimeline.emplace_back(image, newState, time);
}

} // namespace Graphics