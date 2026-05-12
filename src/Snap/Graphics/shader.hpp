#pragma once

#include "Graphics/Buffers/push.hpp"
#include "Graphics/allocations.hpp"
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

namespace Graphics {
struct StructuredBuffer;
}

namespace Graphics::Shader {

struct ShaderExtern {
  std::string name;
  std::string value;

  auto operator==(const ShaderExtern &other) const -> bool {
    return name == other.name && value == other.value;
  }
};

struct ImageTransitionInfo {
  Texture *texture{};
  TextureUsage newUsage = TextureUsage::Unknown;
  // Unused for: Attachments, TransferSrc, TransferDst
  VkPipelineStageFlags2 newStage = VK_PIPELINE_STAGE_2_NONE;
};

static const Type LuaShaderType = Type("Shader");

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

using BoundBufferPair = std::pair<Ref<Buffer>, const Reflect::BufferInfo *>;
using BoundTexturePair = std::pair<Ref<Texture>, const Reflect::SamplerInfo *>;

struct BoundState {
  std::unordered_map<uint64_t, BoundBufferPair> userBoundBuffers;
  std::unordered_map<uint64_t, BoundTexturePair> userBoundTextures;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local std::unordered_map<VkShaderModule, BoundState> BoundStates;
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

  // Set -> (Texture, Sampler)
  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      bindingInfos;

  Reflect::ShaderReflection reflection;
  std::vector<PushBuffer> pushBuffers;

  std::vector<slang::PreprocessorMacroDesc> preprocessorMacros;

  auto GetState() const -> BoundState & {
    return BoundStates.try_emplace(module).first->second;
  }

  std::vector<uint8_t> globalUniforms;

  Math::Uvec3 threadgroupSize{1, 1, 1};
  uint32_t waveSize = 0;

  ~ShaderModule() override {
    auto *ctx = GetCurrentGraphicsContext();

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    if (module != VK_NULL_HANDLE) {
      vkDestroyShaderModule(ctx->device, module, GetAllocationCallbacks());
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
            const Ref<Buffer> &buffer) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key,
            const Ref<Graphics::Texture> &texture) -> Error;
  auto Send(const GraphicsContext &context, const ResourceKey &key,
            const Ref<::Graphics::StructuredBuffer> &buffer) -> Error;

  auto GetUniform(const ResourceKey &key) const
      -> const Reflect::ResourceInfo *;
  auto GetSlotDescription(uint32_t set, uint32_t binding)
      -> const Reflect::ResourceInfo *;
  auto GetSlotDescription(uint64_t slot) -> const Reflect::ResourceInfo *;

  auto GetThreadgroupSize() const -> Result<Math::Uvec3>;
  auto GetWaveSize() const -> uint32_t;

  auto operator==(const ShaderModule &other) const -> bool {
    if (module != other.module) {
      return false;
    }

    if (stages != other.stages) {
      return false;
    }

    if (externs != other.externs) {
      return false;
    }

    return true;
  }

  auto hash() const -> size_t;

  static auto GetType() -> Type const * { return &LuaShaderType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return ShaderModule::GetType();
  }
};

extern Ref<ShaderModule> DefaultShaderModule;       // NOLINT
extern std::vector<const char *> ShaderSearchPaths; // NOLINT

auto LoadModule() -> Error;
void UnloadModule(GraphicsContext &context);

auto AddGlobalShaderExtern(const ShaderExtern &externVar) -> void;

} // namespace Graphics::Shader
