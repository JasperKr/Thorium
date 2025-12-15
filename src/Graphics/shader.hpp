#pragma once

#include "Graphics/Buffers/push.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphics.hpp"
#include "hash.hpp"
#include "reflect.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <string>
#include <unordered_map>
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

static const Type type = Type("Shader");

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
  StructuredBuffer globalUniformBuffer;

  std::unordered_map<uint64_t, StructuredBuffer> uniformBuffers;
  std::unordered_map<uint64_t, StructuredBuffer> storageBuffers;

  std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;
  std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets;

  static auto Create(Graphics::GraphicsContext &context,
                     const std::string &path, const std::string &name)
      -> tl::expected<Ref<ShaderModule>, Error::Error>;

  template <typename T>
  auto Send(GraphicsContext &context, const std::string &name, T &value)
      -> Error::Error {
    for (auto &pushBuffer : pushBuffers) {
      if (pushBuffer.GetLayout().name == name) {
        return pushBuffer.SetData(value);
      }
    }

    // check global ubo
    const auto &layout = globalUniformBuffer.GetLayout();
    switch (layout.type) {
    case BufferResourceType::Unknown:
    case BufferResourceType::Scalar:
    case BufferResourceType::Vector:
    case BufferResourceType::Matrix:
      if (layout.name == name) {
        return globalUniformBuffer.GetBuffer()->SetData(context, value);
      }
    case BufferResourceType::Struct: {
      auto structInfo = std::get<StructInfo>(layout.info);
      auto fieldIt = structInfo.fieldMap.find(name);
      if (fieldIt != structInfo.fieldMap.end()) {
        const auto &fieldInfo = fieldIt->second;
        auto buffer = globalUniformBuffer.GetBuffer();
        return buffer->SetData(context, value, fieldInfo.GetOffset());
      }
    }
    }

    return Error::Create("Push buffer not found: " + name);
  }

  auto Send(GraphicsContext &context, const std::string &name,
            StructuredBuffer &buffer) -> Error::Error {

    for (const auto &resource : reflection.resources) {
      if (resource.variant != ResourceVariant::Buffer) {
        continue;
      }

      const auto &bufferInfo = std::get<BufferInfo>(resource.info);
      if (bufferInfo.name == name) {
        if (descriptorSets[bufferInfo.set] == VK_NULL_HANDLE) {
          return Error::Success(); // Will be created and set later
        }
        // NOLINTNEXTLINE
        auto key = bufferInfo.set | ((uint64_t)bufferInfo.binding << 32U);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer.GetBuffer().get()->handle;
        bufferInfo.offset = 0;
        bufferInfo.range = buffer.GetLayout().size;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[buffer.layout.set];
        descriptorWrite.dstBinding = buffer.layout.binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);

        return Error::Success();
      }
    }

    return Error::Create("Buffer not found in shader reflection: " + name);
  }

  auto Send(GraphicsContext &context, const std::string &name,
            Graphics::Texture::Texture *texture) -> Error::Error {
    for (const auto &resource : reflection.resources) {
      if (resource.variant != ResourceVariant::Sampler) {
        continue;
      }

      const auto &samplerInfo = std::get<SamplerInfo>(resource.info);
      if (resource.name == name) {
        if (descriptorSets[samplerInfo.set] == VK_NULL_HANDLE) {
          return Error::Success(); // Will be created and set later
        }

        // NOLINTNEXTLINE
        auto key = samplerInfo.set | ((uint64_t)samplerInfo.binding << 32U);

        // Create descriptor set for this texture
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture->view;
        imageInfo.sampler = texture->GetSampler(context);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[samplerInfo.set];
        descriptorWrite.dstBinding = samplerInfo.binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);

        return Error::Success();
      }
    }

    return Error::Create("Sampler not found in shader reflection: " + name);
  }

  auto FlushBuffers(GraphicsContext &context, VkPipelineLayout layout)
      -> Error::Error;

  void Destroy(VkDevice device);
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
