#pragma once

#include "Graphics/Buffers/push.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphics.hpp"
#include "reflect.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics::Shader {

struct ShaderExtern {
  std::string name;
  std::string value;

  auto operator==(const ShaderExtern &other) const -> bool {
    return name == other.name && value == other.value;
  }
};

struct ImageTransitionInfo {
  Texture::Texture *texture{};
  Texture::TextureUsage newUsage = Texture::TextureUsage::Unknown;
  // Unused for: Attachments, TransferSrc, TransferDst
  VkPipelineStageFlags2 newStage = VK_PIPELINE_STAGE_2_NONE;
};

struct DescriptorWriteInfo {
  uint32_t dstSet{};
  uint32_t dstBinding{};
  uint32_t dstArrayElement{};
  uint32_t descriptorCount{};
  VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  VkDescriptorImageInfo pImageInfo{};
  VkDescriptorBufferInfo pBufferInfo{};
  VkBufferView pTexelBufferView{};

  Buffer *bufferPtr{};
  Texture::Texture *imagePtr{};
  VkAccessFlagBits2 bufferAccessBits{};

  ImageTransitionInfo transition;

  [[nodiscard]] auto GetWrite(
      const std::unordered_map<uint32_t, VkDescriptorSet> &descriptorSets) const
      -> VkWriteDescriptorSet {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstSet = descriptorSets.at(dstSet);
    write.dstBinding = dstBinding;
    write.dstArrayElement = dstArrayElement;
    write.descriptorCount = descriptorCount;
    write.descriptorType = descriptorType;
    write.pImageInfo = &pImageInfo;
    write.pBufferInfo = &pBufferInfo;
    write.pTexelBufferView = &pTexelBufferView;
    return write;
  }
};

static const Type type = Type("Shader");

constexpr auto
ShaderStageFlagsToPipelineStageFlags(VkShaderStageFlags shaderStages)
    -> VkPipelineStageFlags2 {
  VkPipelineStageFlags2 pipelineStages = 0;

  if (shaderStages == VK_SHADER_STAGE_ALL) {
    return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  }

  if ((shaderStages & VK_SHADER_STAGE_VERTEX_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_GEOMETRY_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_FRAGMENT_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }

  return pipelineStages;
}

struct BoundState {
  std::unordered_map<uint64_t, Ref<Buffer>> boundBuffers;
  std::unordered_map<uint64_t, std::pair<Ref<Buffer>, BufferInfo>>
      userBoundBuffers;

  std::unordered_map<uint64_t, Ref<Texture::Texture>> boundTextures;
  std::unordered_map<uint64_t, std::pair<Ref<Texture::Texture>, SamplerInfo>>
      userBoundTextures;

  std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets;
  std::vector<DescriptorWriteInfo> pendingDescriptorWrites;
  // std::vector<ImageTransitionInfo> pendingImageTransitions;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local std::unordered_map<const struct ShaderModule *, BoundState>
    BoundStates;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

struct ShaderModule : Object {
  ShaderModule() = default;
  ShaderModule(const ShaderModule &) = delete;
  ShaderModule(ShaderModule &&) = delete;
  auto operator=(const ShaderModule &) -> ShaderModule & = delete;
  auto operator=(ShaderModule &&) -> ShaderModule & = delete;
  std::string moduleName;

  VkShaderModule module{};
  std::vector<VkShaderStageFlagBits> stages;

  uint64_t modTime{};

  std::string name;
  std::vector<ShaderExtern> externs;

  slang::ProgramLayout *programLayout = nullptr;
  Slang::ComPtr<slang::IModule> slangModule = nullptr;
  Slang::ComPtr<slang::IComponentType> linkedProgram;

  std::unordered_map<SlangStage, size_t> entryPointToStageIndex;
  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      bindingInfos;
  std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;

  ShaderReflection reflection;
  std::vector<PushBuffer> pushBuffers;

  std::vector<slang::PreprocessorMacroDesc> preprocessorMacros;

  auto GetState() const -> BoundState & {
    return BoundStates.try_emplace(this).first->second;
  }

  std::vector<uint8_t> globalUniforms;

  Math::Uvec3 threadgroupSize{1, 1, 1};
  uint32_t waveSize = 0;

  ~ShaderModule() override {
    auto *ctx = GetCurrentGraphicsContext();

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    // for (auto &pair : GetState().descriptorSetLayouts) {
    //   vkDestroyDescriptorSetLayout(ctx->device, pair.second, nullptr);
    // }

    if (module != VK_NULL_HANDLE) {
      vkDestroyShaderModule(ctx->device, module, nullptr);
      module = VK_NULL_HANDLE;
    }
  }

  static auto
  Create(Graphics::GraphicsContext &context, const std::string &modulename,
         const std::string &name,
         const std::vector<slang::PreprocessorMacroDesc> *preprocessorMacros =
             nullptr) -> Result<Ref<ShaderModule>>;

  auto Send(const GraphicsContext &context, const ResourceKey &key,
            const std::span<const uint8_t> &data) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key,
            StructuredBuffer *buffer) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key,
            const Ref<Graphics::Texture::Texture> &texture) -> Error;

  auto GetUniform(const ResourceKey &key) const -> Result<const ResourceInfo>;
  auto GetSlotDescription(uint32_t set, uint32_t binding)
      -> Result<const ResourceInfo>;
  auto GetSlotDescription(uint64_t slot) -> Result<const ResourceInfo>;

  auto FlushDescriptors(const GraphicsContext &context, VkPipelineLayout layout,
                        VkPipelineStageFlags2 dstStage) -> Error;

  auto GetThreadgroupSize() const -> Result<Math::Uvec3>;
  auto GetWaveSize() const -> uint32_t;

  auto operator==(const ShaderModule &other) const -> bool {
    return module == other.module;
  }

  auto hash() const -> size_t;

  static auto GetType() -> Type const * { return &type; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return ShaderModule::GetType();
  }

  auto ClearBindingCache() const -> void {
    auto &state = GetState();
    state.boundBuffers.clear();
    state.boundTextures.clear();
  }

private:
  auto FlushGlobals(const GraphicsContext &context, VkPipelineLayout layout,
                    VkPipelineStageFlags2 dstStage) -> Error;
};

extern Ref<ShaderModule> DefaultShaderModule; // NOLINT

auto LoadModule() -> Error;
void UnloadModule(GraphicsContext &context);

auto AddGlobalShaderExtern(const ShaderExtern &externVar) -> void;

} // namespace Graphics::Shader
