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
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

#include "vertexformat.hpp"

namespace Graphics::Shader {

struct ShaderExtern {
  std::string name;
  std::string value;

  auto operator==(const ShaderExtern &other) const -> bool {
    return name == other.name && value == other.value;
  }
};

struct DescriptorWriteInfo {
  VkStructureType sType;
  const void *pNext;
  uint32_t dstSet;
  uint32_t dstBinding;
  uint32_t dstArrayElement;
  uint32_t descriptorCount;
  VkDescriptorType descriptorType;
  VkDescriptorImageInfo pImageInfo;
  VkDescriptorBufferInfo pBufferInfo;
  VkBufferView pTexelBufferView;

  Buffer *bufferPtr;
  Texture::Texture *imagePtr;
  VkAccessFlagBits2 bufferAccessBits;

  [[nodiscard]] auto GetWrite(
      const std::unordered_map<uint32_t, VkDescriptorSet> &descriptorSets) const
      -> VkWriteDescriptorSet {
    VkWriteDescriptorSet write{};
    write.sType = sType;
    write.pNext = pNext;
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

struct ImageTransitionInfo {
  Texture::Texture *texture{};
  Texture::TextureUsage newUsage = Texture::TextureUsage::Unknown;
  // Unused for: Attachments, TransferSrc, TransferDst
  VkPipelineStageFlags2 newStage = VK_PIPELINE_STAGE_2_NONE;
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

  ShaderReflection reflection;
  std::vector<PushBuffer> pushBuffers;

  std::unordered_map<uint64_t, Buffer *> boundBuffers;
  std::unordered_map<uint64_t, Texture::Texture *> boundTextures;

  std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;
  std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets;
  std::vector<DescriptorWriteInfo> pendingDescriptorWrites;
  std::vector<ImageTransitionInfo> pendingImageTransitions;

  std::vector<uint8_t> globalUniforms;

  Math::Uvec3 threadgroupSize{1, 1, 1};
  uint32_t waveSize = 0;

  ~ShaderModule() override {
    auto *ctx = GetCurrentGraphicsContext();

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkDestroyShaderModule(ctx->device, module, nullptr);
  }

  static auto Create(Graphics::GraphicsContext &context,
                     const std::string &modulename, const std::string &name)
      -> Result<Ref<ShaderModule>>;

  auto Send(GraphicsContext &context, const ResourceKey &key,
            const std::span<const uint8_t> &data) -> Error;

  auto Send(GraphicsContext &context, const ResourceKey &key,
            StructuredBuffer::StructuredBuffer *buffer) -> Error;

  auto Send(GraphicsContext &context, const ResourceKey &key,
            Graphics::Texture::Texture *texture) -> Error;

  auto GetUniform(const ResourceKey &key) const -> Result<const ResourceInfo>;
  auto GetSlotDescription(uint32_t set, uint32_t binding)
      -> Result<const ResourceInfo>;
  auto GetSlotDescription(uint64_t slot) -> Result<const ResourceInfo>;

  auto FlushBuffers(GraphicsContext &context, VkPipelineLayout layout,
                    VkPipelineStageFlags2 dstStage) -> Error;

  auto GetThreadgroupSize() const -> Result<Math::Uvec3>;
  auto GetWaveSize() const -> uint32_t;

  void Destroy(VkDevice &device);

  auto operator==(const ShaderModule &other) const -> bool {
    return externs == other.externs && moduleName == other.moduleName &&
           stages == other.stages;
  }

  auto hash() const -> size_t;

  static auto GetType() -> Type const * { return &type; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return ShaderModule::GetType();
  }

private:
  auto FlushGlobals(GraphicsContext &context, VkPipelineLayout layout,
                    VkPipelineStageFlags2 dstStage) -> Error;
};

extern Ref<ShaderModule> DefaultShaderModule; // NOLINT

auto LoadModule() -> Error;
void UnloadModule(GraphicsContext &context);

auto AddGlobalShaderExtern(const ShaderExtern &externVar) -> void;

} // namespace Graphics::Shader
