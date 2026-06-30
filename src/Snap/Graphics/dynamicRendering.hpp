#pragma once

#include "Graphics/graphicsState.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Helpers/LRUCache.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphicsContext.hpp"
#include "shader.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
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
extern thread_local std::unordered_map<DescriptorKey, VkDescriptorSet,
                                       DescriptorKeyHash>
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

  std::array<VkColorBlendEquationEXT, MAX_COLOR_ATTACHMENTS>
      colorBlendEquations = {};

  Ref<Shader::ShaderModule> shader;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  std::array<RenderTarget, MAX_COLOR_ATTACHMENTS> colorAttachments;
  RenderTarget depthStencilAttachment;
  bool hasDepthStencilAttachment = false;
  uint8_t colorAttachmentCount = 0;

  mutable uint64_t hash;
  auto GetHash() const -> uint64_t;

  auto operator==(const State &other) const -> bool {
    if (colorAttachmentCount != other.colorAttachmentCount) {
      return false;
    }

    if (*shader != *other.shader) {
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

    for (int i = 0; i < colorAttachmentCount; i++) {
      if (colorAttachments.at(i).texture->getID() !=
          other.colorAttachments.at(i).texture->getID()) {
        return false;
      }
    }

    return true;
  }

  auto ToString() const -> std::string;
};

struct StateKey {
  std::array<RendertargetKey, MAX_COLOR_ATTACHMENTS> colorAttachments{};
  uint32_t colorAttachmentCount;
  bool hasDepthStencilAttachment;
  RendertargetKey depthStencilTextureID{};
  ObjectID shaderModuleID;
  VkPipelineBindPoint bindPoint;
  VkBool32 stencilTestEnable;
  VkPolygonMode polygonMode;
  VkPrimitiveTopology primitiveTopology;

  explicit StateKey(const State &state)
      : colorAttachmentCount(state.colorAttachmentCount),
        hasDepthStencilAttachment(state.hasDepthStencilAttachment),
        shaderModuleID(state.shader->getID()), bindPoint(state.bindPoint),
        stencilTestEnable(state.stencilTestEnable),
        polygonMode(state.polygonMode),
        primitiveTopology(state.primitiveTopology) {

    colorAttachments.fill({0, -1, -1});
    depthStencilTextureID = {.textureID = 0, .location = -1};

    for (int i = 0; i < state.colorAttachmentCount; i++) {
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
    if (colorAttachmentCount != other.colorAttachmentCount ||
        hasDepthStencilAttachment != other.hasDepthStencilAttachment ||
        shaderModuleID != other.shaderModuleID ||
        bindPoint != other.bindPoint ||
        stencilTestEnable != other.stencilTestEnable ||
        polygonMode != other.polygonMode ||
        primitiveTopology != other.primitiveTopology) {
      return false;
    }

    if (hasDepthStencilAttachment &&
        !(depthStencilTextureID == other.depthStencilTextureID)) {
      return false;
    }

    for (int i = 0; i < colorAttachmentCount; i++) {
      if (!(colorAttachments.at(i) == other.colorAttachments.at(i))) {
        return false;
      }
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

    for (int i = 0; i < state.colorAttachmentCount; ++i) {
      hasher.Add(state.colorAttachments.at(i).textureID);
      hasher.Add(state.colorAttachments.at(i).location);
      hasher.Add(state.colorAttachments.at(i).layer);
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
auto SetShader(const Ref<Shader::ShaderModule> &shader) -> void;
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
auto GetShader() -> Ref<Shader::ShaderModule>;
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

} // namespace DynamicRendering
} // namespace Graphics