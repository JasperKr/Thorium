#include "shader.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "graphics.hpp"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"
#include "shaderc/status.h"
#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include "tl/expected.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <print>
#include <string>
#include <vector>

namespace Graphics::Shader {

// TODO: Use Ref<ShaderModule> instead of shader handles
static std::vector<ShaderModule> ShaderModules = {};        // NOLINT
static slang::IGlobalSession *GlobalSlangSession = nullptr; // NOLINT

const std::vector<slang::CompilerOptionEntry> CompilerOptions = {
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

  auto result = slang::createGlobalSession(&GlobalSlangSession);
  if (Error::IsError(result)) {
    std::cerr << "Failed to create Slang global session.\n";
    return;
  }
  SpvTargetDesc.profile = GlobalSlangSession->findProfile("spirv_1_5");
}

static inline auto GetGlobalShaderExterns() -> std::vector<ShaderExtern> & {
  static std::vector<ShaderExtern> GlobalShaderExterns = {};
  return GlobalShaderExterns;
}

auto AddGlobalShaderExtern(const ShaderExtern &externVar) -> void {
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

const std::vector<SlangStage> SlangStages = {
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

void printVarLayout(slang::VariableLayoutReflection *varLayout) {
  if (varLayout->getStage() != SLANG_STAGE_NONE) {
    std::cout << "semantic: \n";
    std::cout << "name: ";
    std::cout << varLayout->getSemanticName() << "\n";
    std::cout << "index: ";
    std::cout << varLayout->getSemanticIndex() << "\n";
  } else {
    std::cout << "No stage semantic.\n";
    std::cout << "name: ";
    std::cout << varLayout->getName() << "\n";
  }
}

void printScope(slang::VariableLayoutReflection *scopeVarLayout) {
  auto *scopeTypeLayout = scopeVarLayout->getTypeLayout();
  switch (scopeTypeLayout->getKind()) {
  case slang::TypeReflection::Kind::Struct: {
    std::print("parameters: \n");

    unsigned int paramCount = scopeTypeLayout->getFieldCount();
    for (int i = 0; i < paramCount; i++) {
      std::print("- ");

      auto *param = scopeTypeLayout->getFieldByIndex(i);
      printVarLayout(param);
    }
  } break;

  case slang::TypeReflection::Kind::None:
  case slang::TypeReflection::Kind::Array:
  case slang::TypeReflection::Kind::Matrix:
  case slang::TypeReflection::Kind::Vector:
  case slang::TypeReflection::Kind::Scalar:
  case slang::TypeReflection::Kind::ConstantBuffer:
  case slang::TypeReflection::Kind::Resource:
  case slang::TypeReflection::Kind::SamplerState:
  case slang::TypeReflection::Kind::TextureBuffer:
  case slang::TypeReflection::Kind::ShaderStorageBuffer:
  case slang::TypeReflection::Kind::ParameterBlock:
  case slang::TypeReflection::Kind::GenericTypeParameter:
  case slang::TypeReflection::Kind::Interface:
  case slang::TypeReflection::Kind::OutputStream:
  case slang::TypeReflection::Kind::Specialized:
  case slang::TypeReflection::Kind::Feedback:
  case slang::TypeReflection::Kind::Pointer:
  case slang::TypeReflection::Kind::DynamicResource:
  case slang::TypeReflection::Kind::MeshOutput:
    std::cout << "Unsupported type layout kind for scope printing.\n";
    break;
  }
}

auto printProgramLayout(slang::ProgramLayout *programLayout) -> void {
  std::print("global scope: ");
  printScope(programLayout->getGlobalParamsVarLayout());

  std::print("entry points: ");
  unsigned int entryPointCount = programLayout->getEntryPointCount();
  for (int i = 0; i < entryPointCount; ++i) {
    std::print("- ");
    const auto *name = programLayout->getEntryPointByIndex(i)->getName();
    std::cout << (name == nullptr ? "<unnamed>" : name);
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
  sessionDesc.targets = &SpvTargetDesc;
  sessionDesc.targetCount = 1;

  slang::ISession *session = nullptr;
  auto result = GlobalSlangSession->createSession(sessionDesc, &session);

  if (Error::IsError(result)) {
    return Error::Create(result);
  }

  Slang::ComPtr<slang::IBlob> diagnosticsBlob;
  std::cout << "Compiling shader: " << shader.moduleName << "\n";

  std::cout << "Loading shader module...\n";
  Slang::ComPtr<slang::IModule> module;

  module = session->loadModule(shader.moduleName.c_str(),
                               diagnosticsBlob.writeRef());

  if (diagnosticsBlob != nullptr) {
    return Error::Create(diagnosticsBlob);
  }
  if (module == nullptr) {
    return Error::Create("Failed to load shader module: " + shader.moduleName);
  }

  auto entryPointCount = module->getDefinedEntryPointCount();
  std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints;
  entryPoints.reserve(entryPointCount);
  shader.stages.reserve(entryPointCount);
  std::vector<SlangStage> stages;
  stages.reserve(entryPointCount);

  std::cout << "Finding shader entry points...\n";

  std::cout << "Shader entry points:\n";
  std::cout << " - Count: " << entryPointCount << "\n";

  auto allowedEntryPointCount = SlangStages.size();
  for (SlangInt32 i = 0; i < allowedEntryPointCount; i++) {
    auto stage = SlangStages.at(i);
    auto entryPointName = SlangStageToString(stage) + "Main";

    Slang::ComPtr<slang::IEntryPoint> entryPoint = nullptr;

    auto result = module->findEntryPointByName(entryPointName.c_str(),
                                               entryPoint.writeRef());

    if (entryPoint.readRef() == nullptr || Error::IsError(result)) {
      continue;
    }

    shader.entryPointToStageIndex[stage] = entryPoints.size();
    entryPoints.emplace_back(entryPoint);

    shader.stages.emplace_back(SlangStageToVkStage(stage));
    stages.emplace_back(stage);

    std::cout << " - " << entryPointName
              << " (stage: " << SlangStageToString(stage) << ")\n";
  }

  std::vector<slang::IComponentType *> componentTypes;
  componentTypes.emplace_back(module);

  for (auto &entryPoint : entryPoints) {
    componentTypes.emplace_back(entryPoint);
  }

  Slang::ComPtr<slang::IComponentType> composedProgram;
  {
    std::cout << "Composing program...\n";
    std::cout << " - Component type count: " << componentTypes.size() << "\n";
    for (const auto &compType : componentTypes) {
      std::cout << "   - " << compType << "\n";
    }

    SlangResult result = session->createCompositeComponentType(
        componentTypes.data(), static_cast<SlangInt>(componentTypes.size()),
        composedProgram.writeRef(), diagnosticsBlob.writeRef());

    std::cout << "Created composite component type.\n";

    auto err =
        Error::Create(result, diagnosticsBlob, composedProgram.readRef());
    if (Error::IsError(err)) {
      return err;
    }
  }

  Slang::ComPtr<slang::IComponentType> linkedProgram;
  {
    std::cout << "Linking program...\n";

    SlangResult result = composedProgram->link(linkedProgram.writeRef(),
                                               diagnosticsBlob.writeRef());

    auto err = Error::Create(result, diagnosticsBlob, linkedProgram.readRef());
    if (Error::IsError(err)) {
      return err;
    }
  }

  shader.programLayout = linkedProgram->getLayout(0);
  Slang::ComPtr<slang::IBlob> spirvCode;

  shader.stages.resize(entryPointCount);
  std::cout << "Getting entry point code"
            << "\n";

  result = linkedProgram->getTargetCode(0, // targetIndex
                                        spirvCode.writeRef(),
                                        diagnosticsBlob.writeRef());

  auto err = Error::Create(result, diagnosticsBlob, spirvCode.readRef());
  if (Error::IsError(err)) {
    return err;
  }

  std::vector<uint32_t> data;
  size_t codeSize = spirvCode->getBufferSize();
  data.resize(codeSize / 4);
  memcpy(data.data(), spirvCode->getBufferPointer(), codeSize);

  // NOLINTNEXTLINE
  std::span<uint8_t> spirvCodeSpan(reinterpret_cast<uint8_t *>(data.data()),
                                   codeSize);

  auto fserr = Filesystem::CreateDirectory(SpirvDirectory);

  if (Error::IsError(fserr)) {
    return fserr;
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
      context.device, &moduleCreateInfo, nullptr, &shader.module));

  if (Error::IsError(error)) {
    return error;
  }

  return Error::Success();
}

auto ShaderModule::Create(Graphics::GraphicsContext &context,
                          const std::string &path, const std::string &name)
    -> tl::expected<Ref<ShaderModule>, Error::Error> {
  ShaderModule shader = {};
  shader.name = name;
  shader.moduleName = path;

  // Compile Slang to SPIR-V and create shader module
  auto error = LoadSlang(context, shader);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  return Ref<ShaderModule>(&shader);
}

} // namespace Graphics::Shader