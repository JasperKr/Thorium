#include "shader.hpp"
#include "Modules/filesystem.hpp"
#include "graphics.hpp"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"
#include "tl/expected.hpp"
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace Graphics::Shader {

const std::string SpirvDirectory = "shaders/spirv/";

void LoadModule() {
  auto err = Filesystem::CreateDirectory(SpirvDirectory);

  if (Error::IsError(err)) {
    std::cerr << "Failed to create SPIR-V directory: " << err.message << "\n";
    return;
  }
}

static inline auto GetGlobalShaderExterns() -> std::vector<ShaderExtern> & {
  static std::vector<ShaderExtern> GlobalShaderExterns = {};
  return GlobalShaderExterns;
}

static auto AddGlobalShaderExtern(const ShaderExtern &externVar) -> void {
  static std::vector<ShaderExtern> &GlobalShaderExterns =
      GetGlobalShaderExterns();
  GlobalShaderExterns.push_back(externVar);
}

inline auto VkShaderStageToShaderCStage(VkShaderStageFlagBits stage)
    -> shaderc_shader_kind {
  switch (stage) {
  case VK_SHADER_STAGE_VERTEX_BIT:
    return shaderc_vertex_shader;
  case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
    return shaderc_tess_control_shader;
  case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
    return shaderc_tess_evaluation_shader;
  case VK_SHADER_STAGE_GEOMETRY_BIT:
    return shaderc_geometry_shader;
  case VK_SHADER_STAGE_FRAGMENT_BIT:
    return shaderc_fragment_shader;
  case VK_SHADER_STAGE_COMPUTE_BIT:
    return shaderc_compute_shader;

  // mesh/task shaders (Vulkan 1.2 / EXT_mesh_shader)
  case VK_SHADER_STAGE_MESH_BIT_EXT:
    return shaderc_mesh_shader; // Shaderc >= 2022
  case VK_SHADER_STAGE_TASK_BIT_EXT:
    return shaderc_task_shader;

  // ray tracing shaders (Vulkan 1.2+ / EXT_ray_tracing)
  case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
    return shaderc_raygen_shader;
  case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
    return shaderc_anyhit_shader;
  case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
    return shaderc_closesthit_shader;
  case VK_SHADER_STAGE_MISS_BIT_KHR:
    return shaderc_miss_shader;
  case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
    return shaderc_intersection_shader;
  case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
    return shaderc_callable_shader;

  default:
    return (shaderc_shader_kind)-1; // invalid
  }
}

static auto GetShaderCCompiler() -> shaderc::Compiler & {
  static shaderc::Compiler compiler = {};

  return compiler;
}

static inline auto LoadSpirV(Graphics::GraphicsContext &context,
                             ShaderModule &shader) -> Error::Error {
  // Load SPIR - V code from file
  auto fileResult =
      Filesystem::ReadFile(Path::Join(SpirvDirectory, shader.spirvPath));
  if (Error::IsError(fileResult)) {
    return fileResult.error();
  }
  auto spirvCode = fileResult.value();

  if (spirvCode.size() == 0) {
    return Error::Create("Shader SPIR-V code is empty: " +
                         Path::Join(SpirvDirectory, shader.spirvPath));
  }

  if (spirvCode.size() % 4 != 0) {
    return Error::Create("Shader SPIR-V code size is not a multiple of 4: " +
                         Path::Join(SpirvDirectory, shader.spirvPath));
  }

  std::vector<uint32_t> spirvInstructions(spirvCode.size() / 4);
  std::memcpy(spirvInstructions.data(), spirvCode.data(), spirvCode.size());

  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = spirvInstructions.size();
  moduleCreateInfo.pCode = spirvInstructions.data();

  Error::Error error = Error::FromVkResult(vkCreateShaderModule(
      context.device, &moduleCreateInfo, nullptr, &shader.module));

  return error;
}

static inline auto LoadGLSL(GraphicsContext &context, ShaderModule &shader) {
  if (shader.code.empty()) {
    return Error::Create("Shader source is empty: " +
                         Path::Join(SpirvDirectory, shader.spirvPath));
  }

  if (shader.spirvPath.empty()) {
    return Error::Create("Shader SPIR-V path is empty.");
  }

  shaderc_shader_kind kind = VkShaderStageToShaderCStage(shader.stage);

  if (kind == (shaderc_shader_kind)-1) {
    return Error::Create("Unsupported shader stage for compilation: " +
                         shader.source.source);
  }

  shaderc::CompileOptions options = shaderc::CompileOptions();
  options.SetTargetEnvironment(shaderc_target_env_vulkan, VK_API_VERSION_1_4);

  shaderc::Compiler &compiler = GetShaderCCompiler();
  auto result =
      compiler.CompileGlslToSpv(shader.code.c_str(), shader.code.size(), kind,
                                shader.source.source.c_str(), "main", options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::string errorMessage =
        "Failed to compile shader: " + shader.source.source + "\n" +
        result.GetErrorMessage();
    return Error::Create(errorMessage);
  }

  std::vector<uint8_t> spirvCode((result.cend() - result.cbegin()) *
                                 sizeof(uint32_t));
  std::memcpy(spirvCode.data(), result.cbegin(), spirvCode.size());

  auto err = Filesystem::CreateDirectory(SpirvDirectory);

  if (Error::IsError(err)) {
    return err;
  }

  std::cout << "Writing SPIR-V to "
            << Path::Join(SpirvDirectory, shader.spirvPath) << "\n";

  err = Filesystem::WriteFile(Path::Join(SpirvDirectory, shader.spirvPath),
                              spirvCode);

  if (Error::IsError(err)) {
    return err;
  }

  std::vector<uint32_t> spirvInstructions(spirvCode.size() / 4);
  std::memcpy(spirvInstructions.data(), spirvCode.data(), spirvCode.size());

  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = spirvInstructions.size() * sizeof(uint32_t);
  moduleCreateInfo.pCode = spirvInstructions.data();
  Error::Error error = Error::FromVkResult(vkCreateShaderModule(
      context.device, &moduleCreateInfo, nullptr, &shader.module));
  return error;
}

template <typename F>
static inline auto TraverseShaderIncludes(const ShaderSource &source,
                                          F &&action) {
  for (const auto &includeSource : source.includeSources) {
    if (action(includeSource)) {
      return true;
    }

    if (TraverseShaderIncludes(includeSource, std::forward<F>(action))) {
      return true;
    }
  }
  return false;
}

// Current line number is 1-based
static inline auto
HandleShaderIncludes(ShaderModule &shader, ShaderSource &currentSource,
                     std::string &line, uint64_t &currentLineNumber)
    -> tl::expected<std::string, Error::Error> {

  // Find #include directives
  const std::string includeDirective = "#include ";

  size_t includePos = line.find(includeDirective);
  if (includePos != std::string::npos) {
    // [#include "filename"] -> extract filename
    size_t start = line.find('"', includePos);
    size_t end = line.find('"', start + 1);

    if (start == std::string::npos || end == std::string::npos ||
        end <= start + 1) {
      return tl::unexpected(
          Error::Create("Invalid #include directive syntax: " + line));
    }

    std::string includeFilename = line.substr(start + 1, end - start - 1);

    // Load included file content

    auto fileResult = Filesystem::ReadTextFile(includeFilename);

    if (Error::IsError(fileResult)) {
      return tl::unexpected(
          Error::Create("Failed to load included file: " + includeFilename));
    }

    ShaderSource newSource = {
        .source = fileResult.value(),
        .includeSources = {},
        .modTime = Filesystem::GetFileModTime(includeFilename),
        .lineCount = 0,
        .includedLineOffset = static_cast<uint32_t>(currentLineNumber),
    };

    currentSource.includeSources.push_back(newSource);

    return PreprocessShaderCode(shader, newSource, currentLineNumber);
  }

  // Empty string to indicate no include found
  return "";
}

// Preprocesses a line of shader code, handling includes and externs
// Current line number is 1-based
static inline auto
PreprocessShaderCodeLine(ShaderModule &shader, ShaderSource &currentSource,
                         std::string &line, uint64_t &currentLineNumber)
    -> tl::expected<std::string, Error::Error> {

  // Handle #include directives
  auto includeResult =
      HandleShaderIncludes(shader, currentSource, line, currentLineNumber);

  if (Error::IsError(includeResult)) {
    return tl::unexpected(includeResult.error());
  }

  if (!includeResult->empty()) {
    return includeResult.value();
  }

  const std::string externDirective = "#defineExtern ";

  size_t externPos = line.find(externDirective);
  if (externPos != std::string::npos) {
    // search through all shader externs for a match
    // if not found, search through global externs
    // [#defineExtern NAME "VALUE NAME"] -> extract NAME and VALUE
    // Becomes #define NAME VALUE
    // For example:
    // #defineExtern MAX_LIGHTS "MAX LIGHTS VALUE"
    // Matches:
    // extern ShaderExtern { name: "MAX_LIGHTS", value: "8" }
    // Becomes:
    // #define MAX_LIGHTS 8

    size_t startName = line.find(' ', externPos + externDirective.size());
    size_t endName = line.find(' ', startName + 1);

    size_t startValueName = line.find('"', endName);
    size_t endValueName = line.find('"', startValueName + 1);

    if (startName == std::string::npos || endName == std::string::npos ||
        endName <= startName + 1 || startValueName == std::string::npos ||
        endValueName == std::string::npos ||
        endValueName <= startValueName + 1) {
      return tl::unexpected(
          Error::Create("Invalid #defineExtern directive syntax: " + line));
    }

    // For example: MAX_LIGHTS
    std::string externName =
        line.substr(startName + 1, endName - startName - 1);
    // For example: MAX LIGHTS VALUE
    std::string externValueName =
        line.substr(startValueName + 1, endValueName - startValueName - 1);
    std::string externValue;

    bool found = false;

    // search local externs
    for (const auto &externVar : shader.externs) {
      if (externVar.name == externValueName) {
        externValue = externVar.value;
        found = true;
        break;
      }
    }

    if (!found) {
      // search global externs
      const auto &globalExterns = GetGlobalShaderExterns();
      for (const auto &externVar : globalExterns) {
        if (externVar.name == externValueName) {
          externValue = externVar.value;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      return tl::unexpected(Error::Create(
          "Shader extern not found for #defineExtern: " + externValueName));
    }

    return "#define " + externName + " " + externValue;
  }

  return line;
}

static inline auto PreprocessShaderCode(ShaderModule &shader,
                                        ShaderSource &source,
                                        uint64_t &currentLineNumber)
    -> tl::expected<std::string, Error::Error> {
  std::istringstream sourceStream(shader.code);
  std::string preprocessedSource;
  std::string line;

  while (std::getline(sourceStream, line)) {
    currentLineNumber++;
    auto result =
        PreprocessShaderCodeLine(shader, source, line, currentLineNumber);

    if (Error::IsError(result)) {
      return tl::unexpected(result.error());
    }

    auto processedLine = result.value();

    preprocessedSource += processedLine + "\n";
  }

  return preprocessedSource;
}

static inline auto LoadCode(ShaderModule &shader) -> Error::Error {
  auto fileResult = Filesystem::ReadTextFile(shader.source.source);

  if (Error::IsError(fileResult)) {
    return fileResult.error();
  }

  shader.code = fileResult.value();

  uint64_t currentLineNumber = 0;

  auto preprocessResult =
      PreprocessShaderCode(shader, shader.source, currentLineNumber);

  if (Error::IsError(preprocessResult)) {
    return preprocessResult.error();
  }

  shader.code = preprocessResult.value();

  return Error::Success();
}

auto ShaderModule::Create(Graphics::GraphicsContext &context,
                          const std::string &path, VkShaderStageFlagBits stage,
                          const std::string &name)
    -> tl::expected<ShaderModule, Error::Error> {
  ShaderModule shader = {};
  shader.stage = stage;
  shader.name = name;
  shader.source = {
      .source = path,
      .includeSources = {},
      .modTime = 0,
      .lineCount = 0,
      .includedLineOffset = 0,
  };

  // if spirvPath exists, load SPIR-V code, else load GLSL code and compile to
  // SPIR-V

  // .fs, .vs, .gs, .cs, etc. -> _fs.spv, _vs.spv, _gs.spv, _cs.spv, etc.
  std::string filename = Path::Filename(path);
  std::string baseName = filename.substr(0, filename.find_last_of('.'));
  std::string extension = Path::Extension(filename);
  baseName += "_" + extension;
  std::string spirvFilename = baseName + ".spv";

  shader.spirvPath = spirvFilename;

  // Check if SPIR-V file exists
#ifdef NDEBUG // Only load SPIR-V in release builds, for faster startup, we
              // cannot assume the SPIR-V is up to date in debug builds
  if (Filesystem::FileExists(spirvFilename)) {
    Error::Error error = LoadSpirV(context, shader);
    if (Error::IsError(error)) {
      return tl::unexpected(error);
    }
    return shader;
  }
#endif
  // Load GLSL code
  Error::Error error = LoadCode(shader);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  // Compile GLSL to SPIR-V and create shader module
  error = LoadGLSL(context, shader);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  return shader;
}

} // namespace Graphics::Shader