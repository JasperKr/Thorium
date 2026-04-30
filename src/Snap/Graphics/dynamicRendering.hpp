#pragma once

#include "Graphics/texture.hpp"
#include "Modules/Helpers/LRU-Cache.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphicsContext.hpp"
#include "shader.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <variant>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics {

namespace Shader {
struct ShaderModule;
}

namespace DynamicRendering {

struct PipelineLayout {
  VkPipelineLayout layout;
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
};

struct ResourceBinding {
  uint32_t binding;
  VkDescriptorType descriptorType;

  std::variant<VkDescriptorImageInfo, VkDescriptorBufferInfo> resourceInfo;
};

struct DescriptorKey {
  VkDescriptorSetLayout layout;
  std::vector<ResourceBinding> bindings; // sorted by binding

  auto operator==(const DescriptorKey &other) const -> bool {
    if (layout != other.layout) {
      return false;
    }

    if (bindings.size() != other.bindings.size()) {
      return false;
    }

    for (size_t i = 0; i < bindings.size(); ++i) {
      const auto &firstBinding = bindings[i];
      const auto &secondBinding = other.bindings[i];

      if (firstBinding.binding != secondBinding.binding) {
        return false;
      }

      if (std::holds_alternative<VkDescriptorImageInfo>(
              firstBinding.resourceInfo) &&
          std::holds_alternative<VkDescriptorImageInfo>(
              secondBinding.resourceInfo)) {
        const auto &firstImageInfo =
            std::get<VkDescriptorImageInfo>(firstBinding.resourceInfo);
        const auto &secondImageInfo =
            std::get<VkDescriptorImageInfo>(secondBinding.resourceInfo);

        if (firstImageInfo.imageView != secondImageInfo.imageView ||
            firstImageInfo.sampler != secondImageInfo.sampler ||
            firstImageInfo.imageLayout != secondImageInfo.imageLayout) {
          return false;
        }
      } else if (std::holds_alternative<VkDescriptorBufferInfo>(
                     firstBinding.resourceInfo) &&
                 std::holds_alternative<VkDescriptorBufferInfo>(
                     secondBinding.resourceInfo)) {
        const auto &firstBufferInfo =
            std::get<VkDescriptorBufferInfo>(firstBinding.resourceInfo);
        const auto &secondBufferInfo =
            std::get<VkDescriptorBufferInfo>(secondBinding.resourceInfo);

        if (firstBufferInfo.buffer != secondBufferInfo.buffer ||
            firstBufferInfo.offset != secondBufferInfo.offset ||
            firstBufferInfo.range != secondBufferInfo.range) {
          return false;
        }
      } else {
        // One is image info, the other is buffer info
        return false;
      }
    }

    return true;
  }
};

struct DescriptorKeyHash {
  auto operator()(const DescriptorKey &key) const noexcept -> size_t {
    Hash::Hasher hasher{};

    hasher.Add(key.layout);

    for (const auto &binding : key.bindings) {
      hasher.Add(binding.binding);

      if (std::holds_alternative<VkDescriptorImageInfo>(binding.resourceInfo)) {
        const auto &imageInfo =
            std::get<VkDescriptorImageInfo>(binding.resourceInfo);
        hasher.Add(imageInfo.imageView);
        hasher.Add(imageInfo.sampler);
        hasher.Add(imageInfo.imageLayout);
      } else if (std::holds_alternative<VkDescriptorBufferInfo>(
                     binding.resourceInfo)) {
        const auto &bufferInfo =
            std::get<VkDescriptorBufferInfo>(binding.resourceInfo);
        hasher.Add(bufferInfo.buffer);
        hasher.Add(bufferInfo.offset);
        hasher.Add(bufferInfo.range);
      }
    }

    return hasher.Get();
  }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

// Only used for cleanup
extern std::mutex PipelinesMutex;
extern std::vector<VkPipeline> Pipelines;

// Only used for cleanup
extern std::mutex PipelineLayoutsMutex;
extern std::vector<PipelineLayout> PipelineLayouts;

extern std::mutex DescriptorSetLayoutCacheMutex;
extern std::unordered_map<struct DescriptorSetLayoutKey, VkDescriptorSetLayout,
                          struct DescriptorSetLayoutKeyHash>
    DescriptorSetLayoutCache;

// Key to cache descriptor sets based on layout and resource pointers
// Immutable pointers but a weak reference is needed to avoid keeping resources alive indefinitely
extern thread_local std::unordered_map<DescriptorKey, VkDescriptorSet,
                                       DescriptorKeyHash>
    DescriptorSetCache;

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

    hasher.Add(key.flags);

    for (const auto &binding : key.bindings) {
      hasher.Add(binding.binding);
      hasher.Add(binding.descriptorType);
      hasher.Add(binding.descriptorCount);
      hasher.Add(binding.stageFlags);

      if (binding.pImmutableSamplers != nullptr) {
        for (uint32_t i = 0; i < binding.descriptorCount; ++i) {
          // NOLINTNEXTLINE, reinterpret cast And pointer arithmetic
          hasher.Add(reinterpret_cast<size_t>(binding.pImmutableSamplers[i]));
        }
      }
    }

    for (VkDescriptorBindingFlags flag : key.bindingFlags) {
      hasher.Add(flag);
    }

    return hasher.Get();
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

extern thread_local State LastStateStorage;
extern thread_local State *LastState;

extern thread_local State *TopOfStack;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto HashBlendmode(VkPipelineColorBlendAttachmentState const &blendMode)
    -> size_t {
  Hash::Hasher hasher{};

  hasher.Add(std::hash<bool>()(blendMode.blendEnable != 0U));
  hasher.Add(std::hash<VkBlendFactor>()(blendMode.srcColorBlendFactor));
  hasher.Add(std::hash<VkBlendFactor>()(blendMode.dstColorBlendFactor));
  hasher.Add(std::hash<VkBlendOp>()(blendMode.colorBlendOp));
  hasher.Add(std::hash<VkBlendFactor>()(blendMode.srcAlphaBlendFactor));
  hasher.Add(std::hash<VkBlendFactor>()(blendMode.dstAlphaBlendFactor));
  hasher.Add(std::hash<VkBlendOp>()(blendMode.alphaBlendOp));
  hasher.Add(std::hash<uint32_t>()(blendMode.colorWriteMask));

  return hasher.Get();
}

inline auto HashTexture(const Texture *texture) -> size_t {
  Hash::Hasher hasher{};

  hasher.Add(std::hash<VkFormat>()(texture->format));
  hasher.Add(std::hash<uint32_t>()(texture->size.width));
  hasher.Add(std::hash<uint32_t>()(texture->size.height));
  hasher.Add(std::hash<uint32_t>()(texture->size.depth));
  hasher.Add(std::hash<uint32_t>()(texture->mipmapcount));
  hasher.Add(std::hash<uint32_t>()(texture->arrayLayers));
  hasher.Add(std::hash<VkImageUsageFlags>()(texture->usage));
  hasher.Add(std::hash<TextureType>()(texture->textureType));

  return hasher.Get();
}

inline auto HashRenderTarget(const RenderTarget *renderTarget) -> size_t {
  Hash::Hasher hasher{};

  hasher.Add(HashBlendmode(renderTarget->blendMode));
  hasher.Add(HashTexture(renderTarget->texture.get()));

  return hasher.Get();
}

struct StateHash {
  static auto Hash(const State &state) -> size_t {
    Hash::Hasher hasher{};

    // Special case for compute pipelines
    if (state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      hasher.Add(std::hash<VkPipelineBindPoint>()(state.bindPoint));
      hasher.Add(state.shader.get() == nullptr ? 0 : state.shader->hash());
      return hasher.Get();
    }

    if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
      PrintError("Trying to hash unsupported pipeline bind point.");
    }

    hasher.Add(std::hash<VkCullModeFlags>()(state.cullMode));
    hasher.Add(std::hash<VkFrontFace>()(state.frontFace));
    hasher.Add(std::hash<bool>()(state.depthTestEnable));
    hasher.Add(std::hash<bool>()(state.depthWriteEnable));
    hasher.Add(std::hash<VkCompareOp>()(state.depthCompareOp));
    hasher.Add(std::hash<bool>()(state.stencilTestEnable));
    hasher.Add(std::hash<VkPolygonMode>()(state.polygonMode));
    hasher.Add(std::hash<float>()(state.lineWidth));
    hasher.Add(state.shader.get() == nullptr ? 0 : state.shader->hash());
    hasher.Add(std::hash<VkPipelineBindPoint>()(state.bindPoint));

    for (const auto &renderTarget : state.renderTargets) {
      hasher.Add(HashRenderTarget(renderTarget.get()));
    }

    return hasher.Get();
  }

  auto operator()(const State &state) const -> size_t {
    return state.GetHash();
  }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern LRUCache<State, std::pair<VkPipeline, PipelineLayout>, StateHash>
    PipelineCache;

extern thread_local std::vector<Ref<Shader::ShaderModule>> UsedShaderModules;
extern thread_local PipelineLayout CurrentPipelineLayout;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

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