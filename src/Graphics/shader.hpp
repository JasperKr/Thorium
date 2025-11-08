#pragma once

#include "Modules/error.hpp"
#include "graphics.hpp"
#include <string>
#include <vector>
#define VK_NO_PROTOTYPES
#include "tl/expected.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics::Shader {

struct ShaderSource {
  std::string source;
  std::vector<ShaderSource> includeSources;
  uint64_t modTime;

  uint32_t lineCount;
  uint32_t includedLineOffset;
};

struct ShaderExtern {
  std::string name;
  std::string value;
};

struct ShaderModule {
  std::string code;
  std::string spirvPath;
  ShaderSource source;

  VkShaderModule module;
  VkShaderStageFlagBits stage;

  uint64_t modTime;

  std::string name;
  std::vector<ShaderExtern> externs;

  static auto Create(Graphics::GraphicsContext &context,
                     const std::string &spirvPath, VkShaderStageFlagBits stage,
                     const std::string &name)
      -> tl::expected<ShaderModule, Error::Error>;

  void Destroy(VkDevice device);
  void ReloadMaybe(Graphics::GraphicsContext &context);
};

void LoadModule();
void UnloadModule();

static inline auto
PreprocessShaderCodeLine(ShaderModule &shader, ShaderSource &currentSource,
                         std::string &line, uint64_t &currentLineNumber)
    -> tl::expected<std::string, Error::Error>;
static inline auto PreprocessShaderCode(ShaderModule &shader,
                                        ShaderSource &currentSource,
                                        uint64_t &currentLineNumber)
    -> tl::expected<std::string, Error::Error>;

static auto AddGlobalShaderExtern(const ShaderExtern &externVar) -> void;

} // namespace Graphics::Shader