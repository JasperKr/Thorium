#pragma once

#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "shader.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace Graphics {

namespace Shader {
struct ShaderModule;
}

namespace RenderTarget {

auto GetSwapchainTextures(const GraphicsContext &context)
    -> Result<std::vector<Ref<Graphics::Texture::Texture>>>;

const static Type type = Type("RenderTarget");

struct RenderTarget : Object {
  VkPipelineColorBlendAttachmentState blendMode = {};
  VkClearValue clearValue = {};
  Ref<Texture::Texture> texture;
  int location = -1; // Default to index in the render target array
  int layer = 0;

  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  auto operator==(const RenderTarget &other) const -> bool {
    return blendMode.blendEnable == other.blendMode.blendEnable &&
           blendMode.srcColorBlendFactor ==
               other.blendMode.srcColorBlendFactor &&
           blendMode.dstColorBlendFactor ==
               other.blendMode.dstColorBlendFactor &&
           blendMode.colorBlendOp == other.blendMode.colorBlendOp &&
           blendMode.srcAlphaBlendFactor ==
               other.blendMode.srcAlphaBlendFactor &&
           blendMode.dstAlphaBlendFactor ==
               other.blendMode.dstAlphaBlendFactor &&
           blendMode.alphaBlendOp == other.blendMode.alphaBlendOp &&
           blendMode.colorWriteMask == other.blendMode.colorWriteMask &&
           texture->format == other.texture->format &&
           texture->size.width == other.texture->size.width &&
           texture->size.height == other.texture->size.height &&
           texture->size.depth == other.texture->size.depth &&
           texture->mipmapcount == other.texture->mipmapcount &&
           texture->arrayLayers == other.texture->arrayLayers &&
           texture->usage == other.texture->usage &&
           texture->textureType == other.texture->textureType &&
           location == other.location;
  }

  static auto GetType() -> Type const * { return &type; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return RenderTarget::GetType();
  }
};

struct SetBindingEntry {
  uint32_t setIndex;
  uint32_t binding;
  std::string name;
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
  VkViewport viewport;
  VkRect2D scissor;

  bool hasViewport = false;
  bool hasScissor = false;

  Ref<Shader::ShaderModule> shader;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VertexFormat vertexFormat;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  std::vector<Ref<RenderTarget>> renderTargets;

  auto operator==(const State &other) const -> bool {
    if (bindPoint != other.bindPoint) {
      return false;
    }

    if (bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      // For compute pipelines, only compare shader
      return shader.get() == other.shader.get();
    }

    if (bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
      PrintError("Comparing unsupported pipeline bind point in state equality");
      return false;
    }

    if (renderTargets.size() != other.renderTargets.size()) {
      PrintAlways("Render target size mismatch in state equality comparison");
      PrintAlways("This size: {}, Other size: {}", renderTargets.size(),
                  other.renderTargets.size());
      return false;
    }

    for (size_t i = 0; i < renderTargets.size(); ++i) {
      if (renderTargets[i].get() == nullptr ||
          other.renderTargets[i].get() == nullptr) {
        PrintWarning(
            "Comparing render targets with null textures in state equality");
        return false;
      }

      if (*renderTargets[i] != *other.renderTargets[i]) {
        return false;
      }
    }

    return cullMode == other.cullMode && frontFace == other.frontFace &&
           depthTestEnable == other.depthTestEnable &&
           depthWriteEnable == other.depthWriteEnable &&
           depthCompareOp == other.depthCompareOp &&
           stencilTestEnable == other.stencilTestEnable &&
           polygonMode == other.polygonMode && lineWidth == other.lineWidth &&
           shader.get() == other.shader.get() &&
           vertexFormat == other.vertexFormat &&
           primitiveTopology == other.primitiveTopology;
  }
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

inline auto HashTexture(const Texture::Texture *texture) -> size_t {
  size_t hash = 0;

  AddToHash(hash, std::hash<VkFormat>()(texture->format));
  AddToHash(hash, std::hash<uint32_t>()(texture->size.width));
  AddToHash(hash, std::hash<uint32_t>()(texture->size.height));
  AddToHash(hash, std::hash<uint32_t>()(texture->size.depth));
  AddToHash(hash, std::hash<uint32_t>()(texture->mipmapcount));
  AddToHash(hash, std::hash<uint32_t>()(texture->arrayLayers));
  AddToHash(hash, std::hash<VkImageUsageFlags>()(texture->usage));
  AddToHash(hash, std::hash<Texture::TextureType>()(texture->textureType));

  return hash;
}

inline auto HashRenderTarget(const RenderTarget *renderTarget) -> size_t {
  size_t hash = 0;

  AddToHash(hash, HashBlendmode(renderTarget->blendMode));
  AddToHash(hash, HashTexture(renderTarget->texture.get()));

  return hash;
}

struct StateHash {
  auto operator()(const State &state) const -> size_t {
    size_t hash = 0;

    constexpr uint32_t prime = 0x9e3779b9;
    constexpr uint32_t shift = 6;
    constexpr uint32_t shift2 = 2;

    // Special case for compute pipelines
    if (state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      AddToHash(hash, std::hash<VkPipelineBindPoint>()(state.bindPoint));
      AddToHash(hash, state.shader.get() == nullptr ? 0 : state.shader->hash());
      return hash;
    }

    if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
      PrintError("Trying to hash unsupported pipeline bind point.");
    }

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
    AddToHash(hash, state.shader.get() == nullptr ? 0 : state.shader->hash());
    AddToHash(hash, std::hash<VkPipelineBindPoint>()(state.bindPoint));

    for (const auto &renderTarget : state.renderTargets) {
      AddToHash(hash, HashRenderTarget(renderTarget.get()));
    }

    return hash;
  }
};

extern std::unordered_map<
    State, std::pair<VkPipeline, VkPipelineLayout>,
    StateHash> // NOLINTNEXTLINE Pipeline cacheBegunRendering
    PipelineCache;

auto FinalizeFrame(GraphicsContext &context) -> Error;
auto BeginFrame(GraphicsContext &context) -> Error;

auto Push(GraphicsContext &context) -> Error;
auto Pop(GraphicsContext &context) -> Error;
auto Reset(GraphicsContext &context) -> Error;
auto FlushGraphics(GraphicsContext &context) -> Result<bool>;
auto Load(GraphicsContext &context) -> Error;
auto Destroy(GraphicsContext &context) -> void;
auto PrepareRendering(GraphicsContext &context) -> Error;

auto EndRendering(GraphicsContext &context) -> void;
auto BeginRendering(GraphicsContext &context) -> Error;

auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp) -> void;
auto SetCullMode(VkCullModeFlags cullMode) -> void;
auto SetPolygonMode(VkPolygonMode polygonMode) -> void;
auto SetViewport(const VkViewport *viewport) -> void;
auto SetScissor(const VkRect2D *scissor) -> void;
auto ClipScissor(const VkRect2D &scissor) -> void;
auto SetShader(const Ref<Shader::ShaderModule> &shader) -> void;
auto SetRenderTargets(const std::vector<Ref<RenderTarget>> &renderTargets)
    -> Error;
auto SetLineWidth(float lineWidth) -> void;
auto SetWindingOrder(VkFrontFace frontFace) -> void;
auto SetVertexFormat(const VertexFormat &vertexFormat) -> void;
auto SetTopology(VkPrimitiveTopology topology) -> void;

auto GetDepthMode() -> std::tuple<bool, bool, VkCompareOp>;
auto GetCullMode() -> VkCullModeFlags;
auto GetPolygonMode() -> VkPolygonMode;
auto GetViewport() -> VkViewport;
auto GetClippedViewport() -> VkViewport;
auto GetMaximumAllowedViewport() -> VkViewport;
auto GetScissor() -> VkRect2D;
auto GetShader() -> Ref<Shader::ShaderModule>;
auto GetRenderTargets() -> std::vector<Ref<RenderTarget>>;
auto GetLineWidth() -> float;
auto GetWindingOrder() -> VkFrontFace;
auto GetVertexFormat() -> VertexFormat;
auto GetTopology() -> VkPrimitiveTopology;
auto SetBindPoint(VkPipelineBindPoint bindPoint) -> void;
auto GetBindPoint() -> VkPipelineBindPoint;

struct ClearInfo {
  std::vector<Color> colors;
  float depthClearValue = 1.0F;
  int stencilClearValue = 0;
  bool clearDepth = false;
  bool clearStencil = false;
};

auto Clear(GraphicsContext &context, const ClearInfo &clearInfo) -> Error;

} // namespace RenderTarget
} // namespace Graphics