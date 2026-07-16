#pragma once

#include "Graphics/graphicsState.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Helpers/LRUCache.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "Modules/type.hpp"
#include "graphicsContext.hpp"
#include "shader.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics::DynamicRendering {

struct PipelineLayout {
  VkPipelineLayout layout;
  Math::StackVector<VkDescriptorSetLayout, 16> descriptorSetLayouts; // NOLINT
};

struct DescriptorKey {
  Math::StackVector<ResourceBinding, 16> bindings; // sorted by binding NOLINT

  auto operator==(const DescriptorKey &other) const -> bool {
    if (bindings.size() != other.bindings.size()) {
      return false;
    }

    return bindings == other.bindings;
  }
};

struct DescriptorKeyHash {
  auto operator()(const DescriptorKey &key) const noexcept -> size_t {
    Hash::Hasher hasher{};

    for (const auto &binding : key.bindings) {
      hasher.Add(binding.binding);
      hasher.Add(binding.resource);
    }

    return hasher.Get();
  }
};

struct Stats {
  uint64_t drawCalls = 0;
  uint64_t dispatchCalls = 0;
  uint64_t triangleCount = 0;
  uint64_t instanceCount = 0;
  uint64_t contextSwitches = 0;

  void Reset() {
    drawCalls = 0;
    dispatchCalls = 0;
    triangleCount = 0;
    instanceCount = 0;
    contextSwitches = 0;
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
extern VkDescriptorSetLayout DefaultEmptySetLayout;

// Key to cache descriptor sets based on layout and resource pointers
// Immutable pointers but a weak reference is needed to avoid keeping resources alive indefinitely
extern thread_local LRUCache<DescriptorKey, VkDescriptorSet, DescriptorKeyHash>
    DescriptorSetCache;

extern thread_local Stats CurrentStats;

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
      [[unlikely]]
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

      [[unlikely]]
      if (binding.pImmutableSamplers != nullptr) {
        // I do not use Immutable samplers, but it is included for completeness, so it is marked as an unlikely branch
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

auto CompareVkPipelineColorBlendAttachmentState(
    const VkPipelineColorBlendAttachmentState &first,
    const VkPipelineColorBlendAttachmentState &second) -> bool;

// NOLINTNEXTLINE
extern thread_local bool DrawnToSwapchain;

const static Type LuaRendertargetType = Type("RenderTarget");

struct RenderTarget {
  VkPipelineColorBlendAttachmentState blendMode = DefaultBlendMode;
  VkClearValue clearValue = {};
  Ref<Texture> texture;
  int location = -1; // Default to index in the render target array
  mutable bool dirty = true;

  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  mutable uint64_t hash;
  auto GetHash() const -> uint64_t;

  auto operator==(const RenderTarget &other) const -> bool {
    if (texture->getID() != other.texture->getID()) {
      return false;
    }

    return location == other.location;
  }
};

struct LuaRenderTarget : Object {
  explicit LuaRenderTarget(RenderTarget renderTarget)
      : renderTarget(std::move(renderTarget)) {}

  RenderTarget renderTarget;

  static auto GetType() -> Type const * { return &LuaRendertargetType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return LuaRenderTarget::GetType();
  }
};

struct SetBindingEntry {
  uint32_t setIndex;
  uint32_t binding;
  std::string name;
};

struct RendertargetKey {
  ObjectID textureID;
  int location;
  int layer;

  auto operator==(const RendertargetKey &other) const -> bool {
    return textureID == other.textureID && location == other.location &&
           layer == other.layer;
  }
};

struct State {
  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
  VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
  VkBool32 depthTestEnable = 1;
  VkBool32 depthWriteEnable = 1;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
  VkBool32 stencilTestEnable = 0;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  VkViewport viewport;
  VkRect2D scissor;

  bool hasViewport = false;
  bool hasScissor = false;
  mutable bool dirty = true;

  Math::StackVector<VkColorBlendEquationEXT, MAX_COLOR_ATTACHMENTS>
      colorBlendEquations;

  Ref<Shader> shader;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  Math::StackVector<RenderTarget, MAX_COLOR_ATTACHMENTS> colorAttachments;
  RenderTarget depthStencilAttachment;
  bool hasDepthStencilAttachment = false;

  mutable uint64_t hash;

  // Incremented each time the state is modified
  mutable uint64_t generation = 0;

  void MarkUpdated() {
    generation++;
    dirty = true;
  }

  auto GetHash() const -> uint64_t;

  auto operator==(const State &other) const -> bool {
    [[likely]]
    if (generation == other.generation) { // quick equal
      return true;
    }

    if (colorAttachments != other.colorAttachments) {
      return false;
    }

    if (shader != other.shader) {
      return false;
    }

    if (stencilTestEnable != other.stencilTestEnable ||
        polygonMode != other.polygonMode ||
        primitiveTopology != other.primitiveTopology ||
        bindPoint != other.bindPoint) {
      return false;
    }

    if (hasDepthStencilAttachment != other.hasDepthStencilAttachment) {
      return false;
    }

    if (hasDepthStencilAttachment) {
      if (depthStencilAttachment != other.depthStencilAttachment) {
        return false;
      }
    }

    return true;
  }

  auto ToString() const -> std::string;
};

struct StateKey {
  Math::StackVector<RendertargetKey, MAX_COLOR_ATTACHMENTS> colorAttachments;
  bool hasDepthStencilAttachment;
  RendertargetKey depthStencilTextureID{};
  ObjectID shaderModuleID;
  VkPipelineBindPoint bindPoint;
  VkBool32 stencilTestEnable;
  VkPolygonMode polygonMode;
  VkPrimitiveTopology primitiveTopology;

  explicit StateKey(const State &state)
      : hasDepthStencilAttachment(state.hasDepthStencilAttachment),
        shaderModuleID(state.shader->getID()), bindPoint(state.bindPoint),
        stencilTestEnable(state.stencilTestEnable),
        polygonMode(state.polygonMode),
        primitiveTopology(state.primitiveTopology) {

    colorAttachments.fill({.textureID = 0, .location = -1, .layer = -1});
    depthStencilTextureID = {.textureID = 0, .location = -1};

    for (int i = 0; i < state.colorAttachments.size(); i++) {
      const auto &attachment = state.colorAttachments.at(i);
      colorAttachments.at(i) = RendertargetKey{
          .textureID = attachment.texture->getID(),
          .location = attachment.location,
      };
    }

    if (state.hasDepthStencilAttachment) {
      depthStencilTextureID = RendertargetKey{
          .textureID = state.depthStencilAttachment.texture->getID(),
          .location = 0,
      };
    }
  }

  auto operator==(const StateKey &other) const -> bool {
    if (hasDepthStencilAttachment != other.hasDepthStencilAttachment ||
        shaderModuleID != other.shaderModuleID ||
        bindPoint != other.bindPoint ||
        stencilTestEnable != other.stencilTestEnable ||
        polygonMode != other.polygonMode ||
        primitiveTopology != other.primitiveTopology ||
        colorAttachments != other.colorAttachments) {
      return false;
    }

    if (hasDepthStencilAttachment &&
        !(depthStencilTextureID == other.depthStencilTextureID)) {
      return false;
    }

    return true;
  }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local std::vector<State> StateStack;

extern thread_local State LastStateStorage;
extern thread_local State *LastState;
extern thread_local State *TopOfStack;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto HashRenderTarget(const RenderTarget &renderTarget) -> size_t {
  Hash::Hasher hasher{};

  hasher.Add(renderTarget.texture->getID());
  hasher.Add(renderTarget.location);

  return hasher.Get();
}

struct StateKeyHash {
  static auto Hash(const StateKey &state) -> size_t {
    Hash::Hasher hasher{};

    // Special case for compute pipelines
    if (state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      hasher.Add(std::hash<VkPipelineBindPoint>()(state.bindPoint));
      hasher.Add(state.shaderModuleID);
      return hasher.Get();
    }

    if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
      PrintError("Trying to hash unsupported pipeline bind point.");
    }

    hasher.Add(std::hash<bool>()(state.stencilTestEnable == 1));
    hasher.Add(std::hash<VkPolygonMode>()(state.polygonMode));
    hasher.Add(std::hash<VkPipelineBindPoint>()(state.bindPoint));
    hasher.Add(std::hash<VkPrimitiveTopology>()(state.primitiveTopology));
    hasher.Add(state.shaderModuleID);

    for (const auto &attachment : state.colorAttachments) {
      hasher.Add(attachment.textureID);
      hasher.Add(attachment.location);
      hasher.Add(attachment.layer);
    }

    return hasher.Get();
  }

  auto operator()(const StateKey &state) const -> size_t { return Hash(state); }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern LRUCache<StateKey, std::pair<VkPipeline, PipelineLayout>, StateKeyHash>
    PipelineCache;

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
void Destroy(const GraphicsContext &context);

// Shuts down the local dynamic rendering module
void Shutdown(const GraphicsContext &context);

auto PrepareRendering(const GraphicsContext &context) -> Error;

auto EndRendering(const GraphicsContext &context) -> void;
auto BeginRendering(const GraphicsContext &context) -> Error;

auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp) -> void;
auto SetCullMode(VkCullModeFlags cullMode) -> void;
auto SetPolygonMode(VkPolygonMode polygonMode) -> void;
auto SetViewport(const VkViewport *viewport) -> void;
auto SetScissor(const VkRect2D *scissor) -> void;
auto ClipScissor(const VkRect2D &scissor) -> void;
auto SetShader(const Ref<Shader> &shader) -> void;
auto SetRenderTargets(const GraphicsContext &context,
                      const std::vector<RenderTarget> &renderTargets) -> Error;
auto SetWindingOrder(VkFrontFace frontFace) -> void;
auto SetTopology(VkPrimitiveTopology topology) -> void;

auto GetDepthMode() -> std::tuple<bool, bool, VkCompareOp>;
auto GetCullMode() -> VkCullModeFlags;
auto GetPolygonMode() -> VkPolygonMode;
auto GetViewport() -> VkViewport;
auto GetClippedViewport() -> VkViewport;
auto GetMaximumAllowedViewport() -> VkViewport;
auto GetScissor() -> VkRect2D;
auto GetShader() -> Ref<Shader>;
auto GetRenderTargets() -> std::vector<RenderTarget>;
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

} // namespace Graphics::DynamicRendering
