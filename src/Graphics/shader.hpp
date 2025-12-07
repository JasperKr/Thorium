#pragma once

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

static const Type type = Type("Shader");

struct ShaderModule : Object {
  std::string moduleName;

  VkShaderModule module;
  std::vector<VkShaderStageFlagBits> stages;

  uint64_t modTime;

  std::string name;
  std::vector<ShaderExtern> externs;

  slang::ProgramLayout *programLayout = nullptr;
  Slang::ComPtr<slang::IModule> slangModule = nullptr;
  Slang::ComPtr<slang::IComponentType> linkedProgram;

  VertexFormats expectedVertexFormat = VertexFormats::Unknown;
  std::unordered_map<SlangStage, size_t> entryPointToStageIndex;

  ShaderReflection reflection;

  static auto Create(Graphics::GraphicsContext &context,
                     const std::string &path, const std::string &name)
      -> tl::expected<Ref<ShaderModule>, Error::Error>;

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
