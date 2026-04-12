#pragma once

#include "Graphics/hash.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphicsContext.hpp"
#include "shader.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics {

namespace Shader {
struct ShaderModule;
}

namespace DynamicRendering {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

// Only used for cleanup
extern std::mutex PipelinesMutex;
extern std::vector<VkPipeline> Pipelines;

// Only used for cleanup
extern std::mutex PipelineLayoutsMutex;
extern std::vector<VkPipelineLayout> PipelineLayouts;

extern std::mutex DescriptorSetLayoutCacheMutex;
extern std::unordered_map<struct DescriptorSetLayoutKey, VkDescriptorSetLayout,
                          struct DescriptorSetLayoutKeyHash>
    DescriptorSetLayoutCache;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

struct DescriptorSetLayoutKey {
  VkDescriptorSetLayoutCreateFlags flags;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<VkDescriptorBindingFlags> bindingFlags;

  auto operator==(const DescriptorSetLayoutKey &other) const -> bool {
    if (flags != other.flags) {
      return false;
    }

    if (bindings.size() != other.bindings.size()) {
      return false;
    }

    for (size_t i = 0; i < bindings.size(); ++i) {
      const auto &firstBinding = bindings[i];
      const auto &secondBinding = other.bindings[i];

      if (firstBinding.binding != secondBinding.binding ||
          firstBinding.descriptorType != secondBinding.descriptorType ||
          firstBinding.descriptorCount != secondBinding.descriptorCount ||
          firstBinding.stageFlags != secondBinding.stageFlags) {
        return false;
      }

      // Compare immutable samplers if they exist
      if (firstBinding.pImmutableSamplers != nullptr &&
          secondBinding.pImmutableSamplers != nullptr) {
        for (uint32_t j = 0; j < firstBinding.descriptorCount; ++j) {
          // NOLINTBEGIN
          if (firstBinding.pImmutableSamplers[j] !=
              secondBinding.pImmutableSamplers[j]) {
            return false;
          }
          // NOLINTEND
        }
      } else if (firstBinding.pImmutableSamplers !=
                 secondBinding.pImmutableSamplers) {
        // One is null, the other is not
        return false;
      }
    }

    if (bindingFlags.size() != other.bindingFlags.size()) {
      return false;
    }

    for (size_t i = 0; i < bindingFlags.size(); ++i) {
      if (bindingFlags[i] != other.bindingFlags[i]) {
        return false;
      }
    }

    return true;
  }
};

struct DescriptorSetLayoutKeyHash {
  auto operator()(DescriptorSetLayoutKey const &key) const noexcept -> size_t {
    Hash::Hasher hasher{};

    hasher.add(key.flags);

    for (const auto &binding : key.bindings) {
      hasher.add(binding.binding);
      hasher.add(binding.descriptorType);
      hasher.add(binding.descriptorCount);
      hasher.add(binding.stageFlags);

      if (binding.pImmutableSamplers != nullptr) {
        for (uint32_t i = 0; i < binding.descriptorCount; ++i) {
          // NOLINTNEXTLINE, reinterpret cast And pointer arithmetic
          hasher.add(reinterpret_cast<size_t>(binding.pImmutableSamplers[i]));
        }
      }
    }

    for (VkDescriptorBindingFlags flag : key.bindingFlags) {
      hasher.add(flag);
    }

    return hasher.get();
  }
};

// NOLINTNEXTLINE
extern thread_local bool DrawnToSwapchain;

const static Type LuaRendertargetType = Type("RenderTarget");

struct RenderTarget : Object {
  VkPipelineColorBlendAttachmentState blendMode = {};
  VkClearValue clearValue = {};
  Ref<Texture> texture;
  int location = -1; // Default to index in the render target array
  int layer = 0;
  mutable bool dirty = true;

  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  mutable uint64_t hash;
  auto GetHash() const -> uint64_t;

  auto operator==(const RenderTarget &other) const -> bool {
    return GetHash() == other.GetHash();
  }

  static auto GetType() -> Type const * { return &LuaRendertargetType; }

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
  mutable bool dirty = true;

  Ref<Shader::ShaderModule> shader;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  std::vector<Ref<RenderTarget>> renderTargets;

  mutable uint64_t hash;
  auto GetHash() const -> uint64_t;

  auto operator==(const State &other) const -> bool {
    return GetHash() == other.GetHash();
  }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local std::vector<State> StateStack;

extern thread_local State LastState;

extern thread_local State *TopOfStack;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto AddToHash(size_t &hash, size_t value) -> void {
  constexpr uint32_t prime = 0x9e3779b9;
  constexpr uint32_t shift = 6;
  constexpr uint32_t shift2 = 2;

  hash ^= value + prime + (hash << shift) + (hash >> shift2);
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

inline auto HashTexture(const Texture *texture) -> size_t {
  size_t hash = 0;

  AddToHash(hash, std::hash<VkFormat>()(texture->format));
  AddToHash(hash, std::hash<uint32_t>()(texture->size.width));
  AddToHash(hash, std::hash<uint32_t>()(texture->size.height));
  AddToHash(hash, std::hash<uint32_t>()(texture->size.depth));
  AddToHash(hash, std::hash<uint32_t>()(texture->mipmapcount));
  AddToHash(hash, std::hash<uint32_t>()(texture->arrayLayers));
  AddToHash(hash, std::hash<VkImageUsageFlags>()(texture->usage));
  AddToHash(hash, std::hash<TextureType>()(texture->textureType));

  return hash;
}

inline auto HashRenderTarget(const RenderTarget *renderTarget) -> size_t {
  size_t hash = 0;

  AddToHash(hash, HashBlendmode(renderTarget->blendMode));
  AddToHash(hash, HashTexture(renderTarget->texture.get()));

  return hash;
}

struct StateHash {
  static auto Hash(const State &state) -> size_t {
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

  auto operator()(const State &state) const -> size_t {
    return state.GetHash();
  }
};

extern thread_local std::unordered_map<
    State, std::pair<VkPipeline, VkPipelineLayout>,
    StateHash> // NOLINTNEXTLINE Pipeline cacheBegunRendering
    PipelineCache;
extern thread_local std::vector<Ref<Shader::ShaderModule>>
    UsedShaderModules;                                      // NOLINT
extern thread_local VkPipelineLayout CurrentPipelineLayout; // NOLINT

auto FinalizeFrame(const GraphicsContext &context) -> Error;
auto BeginFrame(const GraphicsContext &context) -> Error;

auto Push(const GraphicsContext &context) -> Error;
auto Pop(const GraphicsContext &context) -> Error;
auto Reset(const GraphicsContext &context) -> Error;
auto FlushGraphics(const GraphicsContext &context) -> Result<bool>;
auto Load(const GraphicsContext &context) -> Error;

// Destroys all created pipelines and layouts
auto Destroy(const GraphicsContext &context) -> void;

// Shuts down the local dynamic rendering module
auto Shutdown(const GraphicsContext &context) -> Error;

auto PrepareRendering(const GraphicsContext &context) -> Error;

auto EndRendering(const GraphicsContext &context) -> void;
auto BeginRendering(const GraphicsContext &context) -> Error;

auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp) -> void;
auto SetCullMode(VkCullModeFlags cullMode) -> void;
auto SetPolygonMode(VkPolygonMode polygonMode) -> void;
auto SetViewport(const VkViewport *viewport) -> void;
auto SetScissor(const VkRect2D *scissor) -> void;
auto ClipScissor(const VkRect2D &scissor) -> void;
auto SetShader(const Ref<Shader::ShaderModule> &shader) -> void;
auto SetRenderTargets(const GraphicsContext &context,
                      const std::vector<Ref<RenderTarget>> &renderTargets)
    -> Error;
auto SetLineWidth(float lineWidth) -> void;
auto SetWindingOrder(VkFrontFace frontFace) -> void;
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

auto Clear(const GraphicsContext &context, const ClearInfo &clearInfo) -> Error;

auto UsedInPass(const Texture &texture) -> bool;

} // namespace DynamicRendering
} // namespace Graphics