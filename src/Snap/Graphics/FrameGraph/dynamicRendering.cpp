#include "dynamicRendering.hpp"
#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/renderState.hpp"
#include "Modules/Helpers/utils.hpp"
#include "descriptorState.hpp"
#include "pipelineState.hpp"
#include <public/tracy/Tracy.hpp>

namespace Graphics {

inline auto GetRenderExtent(const GraphicsContext &context,
                            const RenderState::State &state) -> VkExtent2D {

  if (!state.colorAttachments.empty()) {
    return VkExtent2D{
        .width = state.colorAttachments.front().texture->GetWidth(),
        .height = state.colorAttachments.front().texture->GetHeight(),
    };
  }

  if (state.hasDepthStencilAttachment) {
    return VkExtent2D{
        .width = state.depthStencilAttachment.texture->GetWidth(),
        .height = state.depthStencilAttachment.texture->GetHeight(),
    };
  }

  assert(false && "Trying to get render extent with no attachments.");
  return VkExtent2D{0, 0};
}

auto compareScissors(const RenderState::State &first,
                     const RenderState::State &second) -> bool {
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

auto compareViewports(const RenderState::State &first,
                      const RenderState::State &second) -> bool {
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

auto compareDepthConfigs(const RenderState::State &first,
                         const RenderState::State &second) -> bool {
  return first.depthTestEnable == second.depthTestEnable &&
         first.depthWriteEnable == second.depthWriteEnable &&
         first.depthCompareOp == second.depthCompareOp;
}

auto compareBlendmodes(const RenderState::State &first,
                       const RenderState::State &second) -> bool {
  if (first.colorAttachments.size() != second.colorAttachments.size()) {
    return false;
  }

  for (size_t i = 0; i < first.colorAttachments.size(); i++) {
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

// NOLINTNEXTLINE
inline auto BeginRendering(const GraphicsContext &context) -> Error {
  ZoneScoped;

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

  renderingInfo.renderArea.offset = {.x = 0, .y = 0};
  renderingInfo.renderArea.extent = GetRenderExtent(context, CurrentState);
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  if (CurrentState.colorAttachments.size() >
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

  for (int i = 0; i < CurrentState.colorAttachments.size(); i++) {
    const auto &rendertarget = CurrentState.colorAttachments.at(i);
    CHECK_ERR(
        rendertarget.texture->UseAsAttachment(context, rendertarget.loadOp));

    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachmentInfo.loadOp = rendertarget.loadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    colorAttachments.at(i) = attachmentInfo;
  }

  bool hasDepth = false;
  bool hasStencil = false;

  if (CurrentState.hasDepthStencilAttachment) {
    auto &rendertarget = CurrentState.depthStencilAttachment;
    CHECK_ERR(
        rendertarget.texture->UseAsAttachment(context, rendertarget.loadOp));

    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
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
  }

#ifndef NDEBUG
  VkExtent2D expectedExtent = GetRenderExtent(context, CurrentState);

  for (const auto &rendertarget : CurrentState.colorAttachments) {
    if (rendertarget.texture->GetWidth() != expectedExtent.width ||
        rendertarget.texture->GetHeight() != expectedExtent.height) {
      return Error::Create(
          "Color attachment extent does not match render area extent.");
    }
  }

  if (hasDepth || hasStencil) {
    if (CurrentState.depthStencilAttachment.texture->GetWidth() !=
            expectedExtent.width ||
        CurrentState.depthStencilAttachment.texture->GetHeight() !=
            expectedExtent.height) {
      return Error::Create(
          "Depth/stencil attachment extent does not match render area extent.");
    }
  }
#endif

  renderingInfo.colorAttachmentCount = CurrentState.colorAttachments.size();
  renderingInfo.pColorAttachments = colorAttachments.data();

  renderingInfo.pStencilAttachment = hasStencil ? &stencilAttachment : nullptr;
  renderingInfo.pDepthAttachment = hasDepth ? &depthAttachment : nullptr;

  if (Graphics::GetVirtualCommandBuffer() == VK_NULL_HANDLE) {
    return Error::Create(
        "Tried to begin rendering, but command buffer is null.");
  }

  // Add a tracy marker to indicate the start of a rendering pass
  TracyMessageL("Begin Rendering");
  // GetCommandBuffer()->BeginRendering(Args::VkCmdBeginRendering{&renderingInfo});
  GetIsCurrentlyRendering() = true;

  // Make sure subsequent renders load from the existing content if we ever need to re-bind mid-pass
  for (auto &rendertarget : CurrentState.colorAttachments) {
    rendertarget.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  if (CurrentState.hasDepthStencilAttachment) {
    CurrentState.depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  return Error::Success();
}

auto EndRendering(const GraphicsContext &context) -> void {
  if (GetIsCurrentlyRendering()) {
    assert(Graphics::GetVirtualCommandBuffer() != nullptr);

    // #if Enable_Snapshots
    //     Snapshot::CaptureEvent(Snapshot::EndRenderingEvent());
    // #endif

    TracyMessageL("End Rendering");
    // vkCmdEndRendering(Graphics::GetCommandBuffer());
    // GetCommandBuffer()->EndRendering({});
    GetIsCurrentlyRendering() = false;
  }
}

// NOLINTNEXTLINE
auto BindDefaultTextures(const GraphicsContext &context, Shader *shader)
    -> Error {
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
      TextureType type = TextureType::ENUM_MAX;

      if ((samplerInfo.shape & SLANG_TEXTURE_3D) == SLANG_TEXTURE_3D) {
        if ((samplerInfo.shape & SLANG_TEXTURE_ARRAY_FLAG) ==
            SLANG_TEXTURE_ARRAY_FLAG) {
          return Error::Create("3D texture arrays are not supported.");
        }
        type = TextureType::VOLUME;
      } else if ((samplerInfo.shape & SLANG_TEXTURE_CUBE) ==
                 SLANG_TEXTURE_CUBE) {
        if ((samplerInfo.shape & SLANG_TEXTURE_ARRAY_FLAG) ==
            SLANG_TEXTURE_ARRAY_FLAG) {
          return Error::Create("Cubemap texture arrays are not supported.");
        }
        type = TextureType::CUBEMAP;
      } else if ((samplerInfo.shape & SLANG_TEXTURE_2D) == SLANG_TEXTURE_2D) {
        if ((samplerInfo.shape & SLANG_TEXTURE_ARRAY_FLAG) ==
            SLANG_TEXTURE_ARRAY_FLAG) {
          type = TextureType::ARRAY;
        } else {
          type = TextureType::DEFAULT;
        }
      }

      auto defaultTexture =
          CHECK_RES(Texture::GetDefault(context, format, type));
      state.userBoundTextures[key] = {defaultTexture, &samplerInfo};
    }
  }

  return Error::Success();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto PrepareRendering(const GraphicsContext &context,
                      VkCommandBuffer vkCommandBuffer) -> Error {
  ZoneScoped;

  bool sameViewport = false;
  bool sameScissor = false;
  bool sameDepth = false;
  bool sameBlendMode = false;
  bool sameCullmode = false;
  bool sameFFWinding = false;

  // Flush updates the last state so we need to compare before updating it
  if (LastState != nullptr) {
    sameViewport = compareViewports(CurrentState, *LastState);
    sameScissor = compareScissors(CurrentState, *LastState);
    sameDepth = compareDepthConfigs(CurrentState, *LastState);
    sameBlendMode = compareBlendmodes(CurrentState, *LastState);
    sameCullmode = CurrentState.cullMode == LastState->cullMode;
    sameFFWinding = CurrentState.frontFace == LastState->frontFace;
  }

  auto updatedState = CHECK_RES(Flush(context, vkCommandBuffer));
  if (updatedState) {
    EndRendering(context);
  }

  {
    assert(GetPipelineCache().currentLayout.layout != nullptr);
    assert(GetVirtualCommandBuffer() != VK_NULL_HANDLE);
    ZoneScopedN("Flush push buffer data");
    for (auto &pushBuffer : CurrentState.shader->pushBuffers) {
      pushBuffer.FlushData(GetPipelineCache().currentLayout.layout);
    }
  }

  auto stages = VK_PIPELINE_STAGE_2_NONE;

  for (const auto &stage : CurrentState.shader->entryPoints) {
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

  CHECK_ERR(BindDefaultTextures(context, CurrentState.shader.get()));
  CHECK_ERR(BindDescriptorSets(context, vkCommandBuffer, stages));

  bool wasRendering = GetIsCurrentlyRendering();
  bool isGraphics = CurrentState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

  if (isGraphics) {
    ZoneScopedN("Dynamic state setup");

    auto *commandBuffer = Graphics::GetVirtualCommandBuffer();

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
      vkCmdSetViewport(vkCommandBuffer, 0, 1, &viewport);
    }

    if (!sameScissor) {
      auto scissor = GetScissor();
      vkCmdSetScissor(vkCommandBuffer, 0, 1, &scissor);
    }

    if (!sameDepth) {
      vkCmdSetDepthTestEnable(vkCommandBuffer, CurrentState.depthTestEnable);
      vkCmdSetDepthWriteEnable(vkCommandBuffer, CurrentState.depthWriteEnable);
      vkCmdSetDepthCompareOp(vkCommandBuffer, CurrentState.depthCompareOp);
    }

    if (!sameBlendMode && CurrentState.colorAttachments.size() > 0) {
      Math::StackVector<VkBool32, MAX_COLOR_ATTACHMENTS> blendEnables = {};
      Math::StackVector<VkColorComponentFlags, MAX_COLOR_ATTACHMENTS>
          colorWriteMasks = {};
      for (const auto &attachment : CurrentState.colorAttachments) {
        blendEnables.emplace_back(attachment.blendMode.blendEnable);
        colorWriteMasks.emplace_back(attachment.blendMode.colorWriteMask);
      }

      vkCmdSetColorBlendEquationEXT(
          vkCommandBuffer, 0,
          static_cast<uint32_t>(CurrentState.colorBlendEquations.size()),
          CurrentState.colorBlendEquations.data());
      vkCmdSetColorBlendEnableEXT(vkCommandBuffer, 0,
                                  static_cast<uint32_t>(blendEnables.size()),
                                  blendEnables.data());
      vkCmdSetColorWriteMaskEXT(vkCommandBuffer, 0,
                                static_cast<uint32_t>(colorWriteMasks.size()),
                                colorWriteMasks.data());
    }

    if (!sameCullmode) {
      vkCmdSetCullMode(vkCommandBuffer, CurrentState.cullMode);
    }

    if (!sameFFWinding) {
      vkCmdSetFrontFace(vkCommandBuffer, CurrentState.frontFace);
    }
  }

  Graphics::GetIsStateDirty() = false;
  return Error::Success();
}

} // namespace Graphics