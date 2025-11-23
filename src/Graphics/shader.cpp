#include "shader.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "graphics.hpp"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"
#include "shaderc/status.h"
#include "slang/slang-com-helper.h"
#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include "tl/expected.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

namespace Graphics::Shader {

static std::vector<ShaderModule> ShaderModules = {};        // NOLINT
static slang::IGlobalSession *GlobalSlangSession = nullptr; // NOLINT

constexpr std::array<slang::CompilerOptionEntry, 2> CompilerOptions = {
    slang::CompilerOptionEntry{
        .name = slang::CompilerOptionName::Optimization,
        .value =
            slang::CompilerOptionValue{
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL,
            }},
    slang::CompilerOptionEntry{
        .name = slang::CompilerOptionName::EmitSpirvMethod,
        .value =
            slang::CompilerOptionValue{
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = SlangEmitSpirvMethod::SLANG_EMIT_SPIRV_DIRECTLY,
            }},
};

// NOLINTNEXTLINE
static slang::TargetDesc SpvTargetDesc = {
    .format = SLANG_SPIRV,
    .profile = SLANG_PROFILE_UNKNOWN,
    .compilerOptionEntries = CompilerOptions.data(),
    .compilerOptionEntryCount = static_cast<uint32_t>(CompilerOptions.size()),
};

const std::string SpirvDirectory = "shaders/spirv/";

void LoadModule() {
  auto err = Filesystem::CreateDirectory(SpirvDirectory);

  if (Error::IsError(err)) {
    std::cerr << "Failed to create SPIR-V directory: " << err.message << "\n";
    return;
  }

  slang::createGlobalSession(&GlobalSlangSession);
  SpvTargetDesc.profile = GlobalSlangSession->findProfile("glsl_450");
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

// static inline auto LoadSpirV(Graphics::GraphicsContext &context,
//                              ShaderModule &shader) -> Error::Error {
//   // Load SPIR - V code from file
//   auto fileResult =
//       Filesystem::ReadFile(Path::Join(SpirvDirectory, shader.spirvPath));
//   if (Error::IsError(fileResult)) {
//     return fileResult.error();
//   }
//   auto spirvCode = fileResult.value();

//   if (spirvCode.size() == 0) {
//     return Error::Create("Shader SPIR-V code is empty: " +
//                          Path::Join(SpirvDirectory, shader.spirvPath));
//   }

//   if (spirvCode.size() % 4 != 0) {
//     return Error::Create("Shader SPIR-V code size is not a multiple of 4: " +
//                          Path::Join(SpirvDirectory, shader.spirvPath));
//   }

//   std::vector<uint32_t> spirvInstructions(spirvCode.size() / 4);
//   memcpy(spirvInstructions.data(), spirvCode.data(), spirvCode.size());

//   VkShaderModuleCreateInfo moduleCreateInfo = {};
//   moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//   moduleCreateInfo.codeSize = spirvInstructions.size();
//   moduleCreateInfo.pCode = spirvInstructions.data();

//   Error::Error error = Error::Create(vkCreateShaderModule(
//       context.device, &moduleCreateInfo, nullptr, &shader.module));

//   return error;
// }

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

auto SlangStageToVkStage(SlangStage stage) {
  switch (stage) {
  case SLANG_STAGE_VERTEX:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case SLANG_STAGE_HULL:
    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  case SLANG_STAGE_DOMAIN:
    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  case SLANG_STAGE_GEOMETRY:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  case SLANG_STAGE_FRAGMENT:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case SLANG_STAGE_COMPUTE:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  case SLANG_STAGE_MESH:
    return VK_SHADER_STAGE_MESH_BIT_EXT;
  case SLANG_STAGE_AMPLIFICATION:
    return VK_SHADER_STAGE_TASK_BIT_EXT;
  case SLANG_STAGE_RAY_GENERATION:
    return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  case SLANG_STAGE_ANY_HIT:
    return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
  case SLANG_STAGE_CLOSEST_HIT:
    return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  case SLANG_STAGE_MISS:
    return VK_SHADER_STAGE_MISS_BIT_KHR;
  case SLANG_STAGE_CALLABLE:
    return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
  default:
    return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  }
}

constexpr std::array<SlangStage, 13> SlangStages = {
    SLANG_STAGE_VERTEX,   SLANG_STAGE_HULL,          SLANG_STAGE_DOMAIN,
    SLANG_STAGE_GEOMETRY, SLANG_STAGE_FRAGMENT,      SLANG_STAGE_COMPUTE,
    SLANG_STAGE_MESH,     SLANG_STAGE_AMPLIFICATION, SLANG_STAGE_RAY_GENERATION,
    SLANG_STAGE_ANY_HIT,  SLANG_STAGE_CLOSEST_HIT,   SLANG_STAGE_MISS,
    SLANG_STAGE_CALLABLE,
};

auto SlangStageToString(SlangStage stage) -> std::string {
  switch (stage) {
  case SLANG_STAGE_VERTEX:
    return "vertex";
  case SLANG_STAGE_HULL:
    return "tessellationControl";
  case SLANG_STAGE_DOMAIN:
    return "tessellationEvaluation";
  case SLANG_STAGE_GEOMETRY:
    return "geometry";
  case SLANG_STAGE_FRAGMENT:
    return "fragment";
  case SLANG_STAGE_COMPUTE:
    return "compute";
  case SLANG_STAGE_MESH:
    return "mesh";
  case SLANG_STAGE_AMPLIFICATION:
    return "task";
  case SLANG_STAGE_RAY_GENERATION:
    return "rayGeneration";
  case SLANG_STAGE_ANY_HIT:
    return "anyHit";
  case SLANG_STAGE_CLOSEST_HIT:
    return "closestHit";
  case SLANG_STAGE_MISS:
    return "miss";
  case SLANG_STAGE_CALLABLE:
    return "callable";
  default:
    return "unknown";
  }
}

static inline auto LoadSlang(GraphicsContext &context, ShaderModule &shader)
    -> Error::Error {
  slang::SessionDesc sessionDesc = {};
  sessionDesc.allowGLSLSyntax = false;
  sessionDesc.defaultMatrixLayoutMode =
      SlangMatrixLayoutMode::SLANG_MATRIX_LAYOUT_ROW_MAJOR;

  auto sourceAndSaveDirectory = std::string(".");
  auto shaderDirectory = Path::Join({"src", "Graphics", "Shaders"});

  std::vector<const char *> searchPaths = {sourceAndSaveDirectory.c_str(),
                                           shaderDirectory.c_str()};

  std::cout << "Shader directories:\n";
  for (const auto &path : searchPaths) {
    std::cout << " - " << path << "\n";
  }

  sessionDesc.searchPaths = searchPaths.data();
  sessionDesc.searchPathCount = static_cast<uint32_t>(searchPaths.size());

  slang::ISession *session = nullptr;
  auto result = GlobalSlangSession->createSession(sessionDesc, &session);

  if (Error::IsError(result)) {
    return Error::Create(result);
  }

  slang::ICompileRequest *compilationRequest = nullptr;
  result = session->createCompileRequest(&compilationRequest);

  if (Error::IsError(result)) {
    return Error::Create(result);
  }

  slang::IBlob *diagnosticsBlob = nullptr;
  std::cout << "Compiling shader: " << shader.moduleName << "\n";
  auto *module =
      session->loadModule(shader.moduleName.c_str(), &diagnosticsBlob);

  if (diagnosticsBlob != nullptr) {
    return Error::Create(diagnosticsBlob);
  }
  if (module == nullptr) {
    return Error::Create("Failed to load shader module: " + shader.moduleName);
  }

  auto entryPointCount = module->getDefinedEntryPointCount();
  std::vector<slang::IEntryPoint *> entryPoints;
  entryPoints.reserve(entryPointCount);
  shader.stages.reserve(entryPointCount);
  std::vector<SlangStage> stages;
  stages.reserve(entryPointCount);

  std::cout << "Shader entry points:\n";
  std::cout << " - Count: " << entryPointCount << "\n";

  auto allowedEntryPointCount = SlangStages.size();

  for (SlangInt32 i = 0; i < allowedEntryPointCount; i++) {
    auto stage = SlangStages.at(i);
    auto entryPointName = SlangStageToString(stage) + "Main";
    slang::IEntryPoint *entryPoint = nullptr;
    auto result =
        module->findEntryPointByName(entryPointName.c_str(), &entryPoint);

    if (entryPoint == nullptr || Error::IsError(result)) {
      continue;
    }
    entryPoints.emplace_back(entryPoint);

    shader.stages.emplace_back(SlangStageToVkStage(stage));
    stages.emplace_back(stage);

    std::cout << " - " << entryPointName
              << " (stage: " << SlangStageToString(stage) << ")\n";
  }

  std::vector<slang::IComponentType *> componentTypes;
  componentTypes.reserve(entryPointCount + 1);
  componentTypes.emplace_back(module);

  for (auto &entryPoint : entryPoints) {
    componentTypes.emplace_back(entryPoint);
  }

  slang::IComponentType *composedProgram = nullptr;
  {
    slang::IBlob *diagnosticsBlob = nullptr;
    SlangResult result = session->createCompositeComponentType(
        componentTypes.data(), static_cast<SlangInt>(componentTypes.size()),
        &composedProgram, &diagnosticsBlob);

    if (diagnosticsBlob != nullptr) {
      return Error::Create(diagnosticsBlob);
    }
    if (result < 0) {
      return Error::Create("Failed to compose program", result);
    }
    if (composedProgram == nullptr) {
      return Error::Create("Composed program is null");
    }
  }

  slang::IComponentType *linkedProgram = nullptr;
  {
    slang::IBlob *diagnosticsBlob = nullptr;
    SlangResult result =
        composedProgram->link(&linkedProgram, &diagnosticsBlob);

    if (diagnosticsBlob != nullptr) {
      return Error::Create(diagnosticsBlob);
    }
    if (result < 0) {
      return Error::Create("Failed to link program", result);
    }
    if (linkedProgram == nullptr) {
      return Error::Create("Linked program is null");
    }
  }

  slang::IBlob *spirvCode = nullptr;

  shader.modules.resize(entryPointCount);
  shader.stages.resize(entryPointCount);
  for (SlangInt32 i = 0; i < entryPointCount; i++) {

    slang::IBlob *diagnosticsBlob = nullptr;
    SlangResult result =
        linkedProgram->getEntryPointCode(i, // entryPointIndex
                                         0, // targetIndex
                                         &spirvCode, &diagnosticsBlob);

    if (diagnosticsBlob != nullptr) {
      return Error::Create(diagnosticsBlob);
    }
    if (result < 0) {
      return Error::Create("Failed to get entry point " + std::to_string(i),
                           result);
    }

    std::vector<uint32_t> data;
    size_t codeSize = spirvCode->getBufferSize();
    data.resize(codeSize / 4);
    memcpy(data.data(), spirvCode->getBufferPointer(), codeSize);

    // NOLINTNEXTLINE
    std::span<uint8_t> spirvCodeSpan(reinterpret_cast<uint8_t *>(data.data()),
                                     codeSize);

    auto err = Filesystem::CreateDirectory(SpirvDirectory);

    if (Error::IsError(err)) {
      return err;
    }

    // auto spirvPath =
    //     shader.moduleName + "_" + SlangStageToString(stages[i]) + ".spv";

    // err = Filesystem::WriteFile(Path::Join(SpirvDirectory, spirvPath),
    //                             spirvCodeSpan);

    // if (Error::IsError(err)) {
    //   return err;
    // }

    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = data.size() * sizeof(uint32_t);
    moduleCreateInfo.pCode = data.data();
    Error::Error error = Error::Create(vkCreateShaderModule(
        context.device, &moduleCreateInfo, nullptr, &shader.modules.at(i)));

    if (Error::IsError(error)) {
      return error;
    }
  }

  return Error::Success();
}

auto ShaderModule::Create(Graphics::GraphicsContext &context,
                          const std::string &path, const std::string &name)
    -> tl::expected<ShaderHandle, Error::Error> {
  ShaderModule &shader = ShaderModules.emplace_back();
  shader.name = name;
  shader.moduleName = path;

  // Compile Slang to SPIR-V and create shader module
  auto error = LoadSlang(context, shader);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  // Shader handles start at 1, to allow the 0 value to represent an invalid
  // handle
  auto shaderHandle = ShaderModules.size();

  return shaderHandle;
}

auto GetShaderModule(const ShaderHandle handle) -> ShaderModule & {
  return ShaderModules[handle - 1];
}

} // namespace Graphics::Shader