#pragma once

#include "Graphics/Buffers/push.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphics.hpp"
#include "hash.hpp"
#include "reflect.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#define VK_NO_PROTOTYPES
#include "tl/expected.hpp"
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
  Texture::Texture *texture;
  Texture::TextureUsage newUsage;
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
  std::string moduleName;

  VkShaderModule module;
  std::vector<VkShaderStageFlagBits> stages;

  uint64_t modTime;

  std::string name;
  std::vector<ShaderExtern> externs;

  auto ScheduleDestroy() -> bool override { return false; };

  slang::ProgramLayout *programLayout = nullptr;
  Slang::ComPtr<slang::IModule> slangModule = nullptr;
  Slang::ComPtr<slang::IComponentType> linkedProgram;

  VertexFormats expectedVertexFormat = VertexFormats::Unknown;
  std::unordered_map<SlangStage, size_t> entryPointToStageIndex;

  ShaderReflection reflection;
  std::vector<PushBuffer> pushBuffers;

  std::unordered_map<uint64_t, StructuredBuffer> uniformBuffers;
  std::unordered_map<uint64_t, StructuredBuffer> storageBuffers;

  std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;
  std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets;
  std::vector<DescriptorWriteInfo> pendingDescriptorWrites;
  std::vector<ImageTransitionInfo> pendingImageTransitions;

  static auto Create(Graphics::GraphicsContext &context,
                     const std::string &modulename, const std::string &name)
      -> tl::expected<Ref<ShaderModule>, Error::Error>;

  auto GetUniformType(const std::string &name) const
      -> tl::expected<ResourceInfo, Error::Error> {
    for (const auto &resource : reflection.resources) {
      if (std::holds_alternative<BufferInfo>(resource.info)) {

        const auto &bufferInfo = std::get<BufferInfo>(resource.info);
        if (bufferInfo.name == name) {
          return resource;
        }
      }
    }

    return Error::Unexpected("Uniform buffer not found: " + name);
  }

  auto Send(GraphicsContext &context, const std::string &name,
            const std::span<const uint8_t> data) -> Error::Error {
    for (auto &pushBuffer : pushBuffers) {
      if (pushBuffer.GetLayout().name == name) {
        return pushBuffer.SetData(name, data);
      }
    }

    // check global ubo
    const auto &layout = reflection.globals;
    if (std::holds_alternative<ScalarInfo>(layout.info) ||
        std::holds_alternative<VectorInfo>(layout.info) ||
        std::holds_alternative<MatrixInfo>(layout.info)) {
      PrintWarning("Sending data to global UBO: {}", layout.name);
      if (layout.name == name) {
        return GetGlobalUniformBuffer().GetBuffer()->SetData(context, data);
      }
    } else if (std::holds_alternative<StructInfo>(layout.info)) {
      auto structInfo = std::get<StructInfo>(layout.info);

      ResourceInfo *field = nullptr;

      for (auto &currentField : structInfo.fields) {
        if (currentField.name == name) {
          field = &currentField;
          break;
        }
      }

      if (field != nullptr) {
        auto buffer = GetGlobalUniformBuffer().GetBuffer();
        return buffer->SetData(context, data, field->offset);
      }
    }

    return Error::Create("Uniform not found: " + name);
  }

  auto Send(GraphicsContext &context, const std::string &name, Buffer *buffer)
      -> Error::Error {

    for (const auto &resource : reflection.resources) {
      if (!std::holds_alternative<BufferInfo>(resource.info)) {
        continue;
      }

      const auto &bufferInfo = std::get<BufferInfo>(resource.info);
      if (bufferInfo.name == name) {
        if (descriptorSets[bufferInfo.set] == VK_NULL_HANDLE) {
          return Error::Success(); // Will be created and set later
        }
        // NOLINTNEXTLINE
        auto key = bufferInfo.set | ((uint64_t)bufferInfo.binding << 32U);

        VkDescriptorBufferInfo vkBufferInfo{};
        vkBufferInfo.buffer = buffer->handle;
        vkBufferInfo.offset = 0;
        vkBufferInfo.range = buffer->size;

        DescriptorWriteInfo descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = bufferInfo.set;
        descriptorWrite.dstBinding = bufferInfo.binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = vkBufferInfo;

        // vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
        pendingDescriptorWrites.emplace_back(descriptorWrite);

        return Error::Success();
      }
    }

    return Error::Create("Buffer not found in shader reflection: " + name);
  }

  auto Send(GraphicsContext &context, const std::string &name,
            Graphics::Texture::Texture *texture) -> Error::Error {
    for (const auto &resource : reflection.resources) {
      if (!std::holds_alternative<SamplerInfo>(resource.info)) {
        continue;
      }

      const auto &samplerInfo = std::get<SamplerInfo>(resource.info);
      if (resource.name == name) {
        // NOLINTNEXTLINE
        auto key = samplerInfo.set | ((uint64_t)samplerInfo.binding << 32U);

        // Create descriptor set for this texture
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture->view;
        imageInfo.sampler = texture->GetSampler(context);

        DescriptorWriteInfo descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = samplerInfo.set;
        descriptorWrite.dstBinding = samplerInfo.binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = imageInfo;

        pendingDescriptorWrites.emplace_back(descriptorWrite);
        pendingImageTransitions.emplace_back(ImageTransitionInfo{
            .texture = texture,
            .newUsage = Texture::TextureUsage::Sampler,
            .newStage = ShaderStageFlagsToPipelineStageFlags(resource.stages),
        });

        return Error::Success();
      }
    }

    return Error::Create("Sampler not found in shader reflection: " + name);
  }

  auto FlushBuffers(GraphicsContext &context, VkPipelineLayout layout)
      -> Error::Error;

  void Destroy(VkDevice &device);
  void ReloadMaybe(Graphics::GraphicsContext &context);

  [[nodiscard]] auto GetExpectedVertexFormat() const -> VertexFormats {
    return expectedVertexFormat;
  }

  auto operator==(const ShaderModule &other) const -> bool {
    return externs == other.externs && moduleName == other.moduleName &&
           stages == other.stages;
  }

  auto hash() const -> size_t {
    Hash::Hasher hasher;
    hasher.add(std::hash<std::string>()(moduleName));
    for (const auto &stage : stages) {
      hasher.add(static_cast<uint32_t>(stage));
    }
    for (const auto &externVar : externs) {
      hasher.add(std::hash<std::string>()(externVar.name));
      hasher.add(std::hash<std::string>()(externVar.value));
    }

    return hasher.get();
  }

  static auto GetType() -> Type const * { return &type; }
};

extern Ref<ShaderModule> DefaultShaderModule; // NOLINT

auto LoadModule() -> Error::Error;
void UnloadModule();

auto AddGlobalShaderExtern(const ShaderExtern &externVar) -> void;

} // namespace Graphics::Shader
