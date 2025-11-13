#include "shader.hpp"
#include "Modules/filesystem.hpp"
#include "graphics.hpp"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"
#include "shaderc/status.h"
#include "tl/expected.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
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
  GlobalShaderExterns.emplace_back(externVar);
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
  memcpy(spirvInstructions.data(), spirvCode.data(), spirvCode.size());

  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = spirvInstructions.size();
  moduleCreateInfo.pCode = spirvInstructions.data();

  Error::Error error = Error::FromVkResult(vkCreateShaderModule(
      context.device, &moduleCreateInfo, nullptr, &shader.module));

  return error;
}

template <typename F>
static inline auto TraverseShaderIncludes(const ShaderSource &source,
                                          F &&action) -> void {
  action(source);

  // NOLINTNEXTLINE
  for (const auto &includeSource : source.includeSources) {
    TraverseShaderIncludes(includeSource, std::forward<F>(action));
  }
}

static inline auto
SpvCompilationStatusToString(const shaderc_compilation_status result)
    -> std::string {
  switch (result) {
  case shaderc_compilation_status_success:
    return "Compilation succeeded.";
  case shaderc_compilation_status_invalid_stage:
    return "Invalid shader stage.";
  case shaderc_compilation_status_compilation_error:
    return "Compilation error: ";
  case shaderc_compilation_status_internal_error:
    return "Internal compiler error: ";
  case shaderc_compilation_status_null_result_object:
    return "Null result object.";
  case shaderc_compilation_status_invalid_assembly:
    return "Invalid SPIR-V assembly.";
  case shaderc_compilation_status_validation_error:
    return "SPIR-V validation error.";
  case shaderc_compilation_status_transformation_error:
    return "SPIR-V transformation error.";
  case shaderc_compilation_status_configuration_error:
    return "Configuration error.";
  default:
    return "Unknown compilation status.";
  }
}

enum class ConsoleColor : uint8_t {
  Red,
  Green,
  Blue,
  Yellow,
  Cyan,
  Magenta,
  White,
  Reset
};

auto GetColorCode(ConsoleColor color) -> std::string {
  switch (color) {
  case ConsoleColor::Red:
    return "\033[31m";
  case ConsoleColor::Green:
    return "\033[32m";
  case ConsoleColor::Blue:
    return "\033[34m";
  case ConsoleColor::Yellow:
    return "\033[33m";
  case ConsoleColor::Cyan:
    return "\033[36m";
  case ConsoleColor::Magenta:
    return "\033[35m";
  case ConsoleColor::White:
    return "\033[37m";
  default:
    return "\033[0m";
  }
}

static inline auto
HandleCompilationError(const shaderc::SpvCompilationResult &result,
                       const ShaderModule &shader) -> std::string {
  shaderc_compilation_status status = result.GetCompilationStatus();

  // We should only handle compilation errors here
  if (status != shaderc_compilation_status_compilation_error) {
    return SpvCompilationStatusToString(status);
  }

  // Find first : to get the line number
  // Example error message:
  // Error: src/Graphics/Shaders/default.vs:14: error: '#error' :

  auto errorMessage = result.GetErrorMessage();
  size_t firstColon = errorMessage.find(':');
  if (firstColon == std::string::npos) {
    return "Unknown compilation error.";
  }

  size_t secondColon = errorMessage.find(':', firstColon + 1);
  if (secondColon == std::string::npos) {
    return "Unknown compilation error.";
  }

  std::string lineNumberStr =
      errorMessage.substr(firstColon + 1, secondColon - firstColon - 1);
  uint64_t lineNumber = std::stoull(lineNumberStr);

  /* Traverse includes to find the original source and line number
  Current line number is 1-based
  We can get the shader source object we included from by traversing the
  includes And checking the includedLineOffset and code line count and if the
  error line number falls within that range then we can adjust the line number
  accordingly, choosing the deepest include that matches the line number
  */

  ShaderSource originalSource = shader.source;
  std::string includeTree;

  bool rootFile = true;

  // Export to file for easier debugging

  Filesystem::WriteFile("shader_source_with_error.txt", shader.code);

  TraverseShaderIncludes(
      shader.source, [&](const ShaderSource &source) -> void {
        uint32_t startLine = source.includedLineOffset;  // inclusive
        uint32_t endLine = startLine + source.lineCount; // inclusive

        if (lineNumber >= startLine && lineNumber <= endLine) {
          if (!rootFile) {
            includeTree += "Included by: " + originalSource.source + ": ";
            includeTree += std::to_string(source.includedLineOffset -
                                          originalSource.includedLineOffset) +
                           "\n";
          }

          originalSource = source;
          rootFile = false;
        }

        if (rootFile) {
          std::cout << "Error handling shader error message: Error outside of "
                       "root file range.\n";
          rootFile = false;
        }
      });

  lineNumber -= originalSource.includedLineOffset;

  std::string error =
      GetColorCode(ConsoleColor::Red) + "Shader Compilation Error\n";

  error += GetColorCode(ConsoleColor::Cyan) + includeTree;
  error += GetColorCode(ConsoleColor::Green) + originalSource.source + ": " +
           std::to_string(lineNumber) + "\n";
  error += GetColorCode(ConsoleColor::Reset);

  std::string errmsgWithoutFileInfo =
      errorMessage.substr(secondColon + 1); // skip past line number info

  error += errmsgWithoutFileInfo;

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
    std::string errorMessage = HandleCompilationError(result, shader);
    return Error::Create(errorMessage);
  }

  std::vector<uint8_t> spirvCode((result.cend() - result.cbegin()) *
                                 sizeof(uint32_t));
  memcpy(spirvCode.data(), result.cbegin(), spirvCode.size());

  auto err = Filesystem::CreateDirectory(SpirvDirectory);

  if (Error::IsError(err)) {
    return err;
  }

  err = Filesystem::WriteFile(Path::Join(SpirvDirectory, shader.spirvPath),
                              spirvCode);

  if (Error::IsError(err)) {
    return err;
  }

  std::vector<uint32_t> spirvInstructions(spirvCode.size() / 4);
  memcpy(spirvInstructions.data(), spirvCode.data(), spirvCode.size());

  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = spirvInstructions.size() * sizeof(uint32_t);
  moduleCreateInfo.pCode = spirvInstructions.data();
  Error::Error error = Error::FromVkResult(vkCreateShaderModule(
      context.device, &moduleCreateInfo, nullptr, &shader.module));
  return error;
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
        .source = includeFilename,
        .code = fileResult.value(),
        .includeSources = {},
        .modTime = Filesystem::GetFileModTime(includeFilename),
        .lineCount = 0,
        .includedLineOffset = static_cast<uint32_t>(currentLineNumber),
    };

    currentSource.includeSources.emplace_back(newSource);
    auto &pushedSource = currentSource.includeSources.back();

    uint64_t previousLineNumber = currentLineNumber;

    auto code = PreprocessShaderCode(shader, pushedSource, currentLineNumber);

    uint32_t includedLineCount = currentLineNumber - previousLineNumber;
    pushedSource.lineCount = includedLineCount;

    if (Error::IsError(code)) {
      return tl::unexpected(code.error());
    }

    return "//// Include: " + includeFilename + " ////\n" + code.value();
  }

  // Empty string to indicate no include found
  return "No Include";
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

  if (includeResult.value() != "No Include") {
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

    return "#define " + externName + " " + externValue + "\n";
  }

  return line + "\n";
}

static inline auto PreprocessShaderCode(ShaderModule &shader,
                                        ShaderSource &source,
                                        uint64_t &currentLineNumber)
    -> tl::expected<std::string, Error::Error> {
  std::istringstream sourceStream(source.code);
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

    preprocessedSource += processedLine;
  }

  return preprocessedSource;
}

static inline auto LoadCode(ShaderModule &shader) -> Error::Error {
  auto fileResult = Filesystem::ReadTextFile(shader.source.source);

  if (Error::IsError(fileResult)) {
    return fileResult.error();
  }

  shader.source.code = fileResult.value();

  uint64_t currentLineNumber = 0;

  auto preprocessResult =
      PreprocessShaderCode(shader, shader.source, currentLineNumber);

  if (Error::IsError(preprocessResult)) {
    return preprocessResult.error();
  }

  shader.code = preprocessResult.value();
  shader.source.lineCount = static_cast<uint32_t>(currentLineNumber);

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