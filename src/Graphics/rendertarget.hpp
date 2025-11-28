#pragma once

#include "Graphics/graphics.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <vulkan/vulkan_core.h>
namespace Graphics::RenderTarget {

struct RenderTarget {
  VkPipelineColorBlendAttachmentState blendMode = {};
  Texture::Texture texture = {};
  int location = -1; // Default to index in the render target array
};

struct State {
  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
  VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
  bool depthTestEnable = true;
  bool depthWriteEnable = true;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
  bool stencilTestEnable = false;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  float lineWidth = 1.0F;
  VkViewport viewport = {};
  VkRect2D scissor = {};

  Shader::ShaderHandle shader = {};

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  std::vector<RenderTarget> renderTargets;
};

inline auto AddToHash(size_t hash, size_t value) -> size_t {
  constexpr uint32_t prime = 0x9e3779b9;
  constexpr uint32_t shift = 6;
  constexpr uint32_t shift2 = 2;

  hash ^= value + prime + (hash << shift) + (hash >> shift2);
  return hash;
}

inline auto HashBlendmode(VkPipelineColorBlendAttachmentState const &blendMode)
    -> size_t {
  size_t hash = 0;

  AddToHash(hash, std::hash<bool>()(blendMode.blendEnable != 0U));
  AddToHash(hash, std::hash<VkBlendFactor>()(blendMode.srcColorBlendFactor));
  AddToHash(hash, std::hash<VkBlendFactor>()(blendMode.dstColorBlendFactor));
  AddToHash(hash, std::hash<VkBlendOp>()(blendMode.colorBlendOp));
  AddToHash(hash, std::hash<VkBlendFactor>()(blendMode.srcAlphaBlendFactor));
  AddToHash(hash, std::hash<VkBlendFactor>()(blendMode.dstAlphaBlendFactor));
  AddToHash(hash, std::hash<VkBlendOp>()(blendMode.alphaBlendOp));
  AddToHash(hash, std::hash<uint32_t>()(blendMode.colorWriteMask));

  return hash;
}

inline auto HashTexture(const Texture::Texture &texture) -> size_t {
  size_t hash = 0;

  AddToHash(hash, std::hash<VkFormat>()(texture.format));
  AddToHash(hash, std::hash<uint32_t>()(texture.size.width));
  AddToHash(hash, std::hash<uint32_t>()(texture.size.height));
  AddToHash(hash, std::hash<uint32_t>()(texture.size.depth));
  AddToHash(hash, std::hash<uint32_t>()(texture.mipmapcount));
  AddToHash(hash, std::hash<uint32_t>()(texture.arrayLayers));
  AddToHash(hash, std::hash<VkImageUsageFlags>()(texture.usage));
  AddToHash(hash, std::hash<Texture::TextureType>()(texture.type));

  return hash;
}

inline auto HashRenderTarget(const RenderTarget &renderTarget) -> size_t {
  size_t hash = 0;

  AddToHash(hash, HashBlendmode(renderTarget.blendMode));
  AddToHash(hash, HashTexture(renderTarget.texture));

  return hash;
}

struct StateHash {
  auto operator()(const State &state) const -> size_t {
    size_t hash = 0;

    constexpr uint32_t prime = 0x9e3779b9;
    constexpr uint32_t shift = 6;
    constexpr uint32_t shift2 = 2;

    AddToHash(hash, std::hash<VkCullModeFlags>()(state.cullMode));
    AddToHash(hash, std::hash<VkFrontFace>()(state.frontFace));
    AddToHash(hash, std::hash<bool>()(state.depthTestEnable));
    AddToHash(hash, std::hash<bool>()(state.depthWriteEnable));
    AddToHash(hash, std::hash<VkCompareOp>()(state.depthCompareOp));
    AddToHash(hash, std::hash<bool>()(state.stencilTestEnable));
    AddToHash(hash, std::hash<VkPolygonMode>()(state.polygonMode));
    AddToHash(hash, std::hash<float>()(state.lineWidth));
    AddToHash(hash, std::hash<float>()(state.viewport.x));
    AddToHash(hash, std::hash<float>()(state.viewport.y));
    AddToHash(hash, std::hash<float>()(state.viewport.width));
    AddToHash(hash, std::hash<float>()(state.viewport.height));
    AddToHash(hash, std::hash<int32_t>()(state.scissor.offset.x));
    AddToHash(hash, std::hash<int32_t>()(state.scissor.offset.y));
    AddToHash(hash, std::hash<uint32_t>()(state.scissor.extent.width));
    AddToHash(hash, std::hash<uint32_t>()(state.scissor.extent.height));
    AddToHash(hash, std::hash<Shader::ShaderHandle>()(state.shader));
    AddToHash(hash, std::hash<VkPipelineBindPoint>()(state.bindPoint));

    for (const auto &renderTarget : state.renderTargets) {
      AddToHash(hash, HashRenderTarget(renderTarget));
    }

    return hash;
  }
};

// NOLINTNEXTLINE Pipeline cache
static std::map<State, VkPipeline, StateHash> PipelineCache = {};

} // namespace Graphics::RenderTarget