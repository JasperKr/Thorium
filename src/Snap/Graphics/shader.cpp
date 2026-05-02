#include "shader.hpp"
#include "Graphics/Buffers/push.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/snapshot.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "graphics.hpp"
#include "shaderc/shaderc.h"
#include "shaderc/shaderc.hpp"
#include "shaderc/status.h"
#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include "tl/expected.hpp"
#include <array>
#include <format>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string_view>
#include <utility>

#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Graphics::Shader {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
thread_local std::unordered_map<VkShaderModule, BoundState> BoundStates;
static slang::IGlobalSession *GlobalSlangSession = nullptr;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static const std::vector<slang::CompilerOptionEntry> CompilerOptions = {
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
Ref<ShaderModule> DefaultShaderModule = {}; // NOLINT

auto LoadModule() -> Error {
  auto err = Filesystem::CreateDirectory(SpirvDirectory);

  if (Error::IsError(err)) {
    return err;
  }

  auto result = slang::createGlobalSession(&GlobalSlangSession);
  if (Error::IsError(result)) {
    return Error::Create(result);
  }
  SpvTargetDesc.profile = GlobalSlangSession->findProfile("spirv_1_5");

  auto shaderCreationResult = ShaderModule::Create(
      *GetCurrentGraphicsContext(), "default2D", "Default shader");

  if (Error::IsError(shaderCreationResult)) {
    return shaderCreationResult.error();
  }

  DefaultShaderModule = shaderCreationResult.value();

  return Error::Success();
}

void UnloadModule(Graphics::GraphicsContext &context) {
  DefaultShaderModule.reset();

  if (GlobalSlangSession != nullptr) {
    GlobalSlangSession->release();
  }

  slang::shutdown();
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

static inline auto
SpvCompilationStatusToString(const shaderc_compilation_status result)
    -> std::string_view {
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

auto SlangStageToString(SlangStage stage) -> std::string_view {
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

static inline auto LoadSlang(GraphicsContext &context,
                             Ref<ShaderModule> &shader) -> Error {
  slang::SessionDesc sessionDesc = {};
  sessionDesc.allowGLSLSyntax = false;
  sessionDesc.defaultMatrixLayoutMode =
      SlangMatrixLayoutMode::SLANG_MATRIX_LAYOUT_ROW_MAJOR;

  auto sourceBaseDir = Filesystem::GetSourceBaseDirectory();
  auto sourceDir = Filesystem::GetSourceDirectory();
  auto saveDir = Filesystem::GetSaveDirectory();

  auto shaderDirectory = Path::Join({"src", "Snap", "Graphics", "Shaders"});
  const auto &sourceBack = Path::Join(sourceDir, std::string(".."));

  std::vector<const char *> searchPaths = {
      sourceDir.c_str(), sourceBack.c_str(), saveDir.c_str(),
      sourceBaseDir.c_str(), shaderDirectory.c_str()};

  std::string directories = "To be searched:\n";
  for (const auto &path : searchPaths) {
    directories += " - " + std::string(path) + "\n";
  }

  PrintDebug(directories);

  sessionDesc.searchPaths = searchPaths.data();
  sessionDesc.searchPathCount = static_cast<uint32_t>(searchPaths.size());
  sessionDesc.targets = &SpvTargetDesc;
  sessionDesc.targetCount = 1;
  sessionDesc.preprocessorMacros = shader->preprocessorMacros.data();
  sessionDesc.preprocessorMacroCount =
      static_cast<uint32_t>(shader->preprocessorMacros.size());

  slang::ISession *session = nullptr;
  auto result = GlobalSlangSession->createSession(sessionDesc, &session);

  if (Error::IsError(result)) {
    return Error::Create(result);
  }

  Slang::ComPtr<slang::IBlob> diagnosticsBlob;
  PrintDebug("Compiling shader: " + shader->moduleName);

  shader->slangModule = session->loadModule(shader->moduleName.c_str(),
                                            diagnosticsBlob.writeRef());

  if (diagnosticsBlob != nullptr) {
    return Error::Create(diagnosticsBlob);
  }
  if (shader->slangModule == nullptr) {
    return Error::Create("Failed to load shader module: " + shader->moduleName);
  }

  auto entryPointCount = shader->slangModule->getDefinedEntryPointCount();
  std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints;
  entryPoints.reserve(entryPointCount);
  shader->stages.reserve(entryPointCount);
  std::vector<SlangStage> stages;
  stages.reserve(entryPointCount);

  PrintDebug("Shader entry points:");
  PrintDebug(" - Count: " + std::to_string(entryPointCount));

  auto allowedEntryPointCount = SlangStages.size();
  for (SlangInt32 i = 0; i < allowedEntryPointCount; i++) {
    auto stage = SlangStages.at(i);
    // auto entryPointName = SlangStageToString(stage) + "Main";
    auto entryPointName = std::format("{}Main", SlangStageToString(stage));

    Slang::ComPtr<slang::IEntryPoint> entryPoint = nullptr;

    auto result = shader->slangModule->findEntryPointByName(
        entryPointName.c_str(), entryPoint.writeRef());

    if (entryPoint.readRef() == nullptr || Error::IsError(result)) {
      continue;
    }

    shader->entryPointToStageIndex[stage] = entryPoints.size();
    entryPoints.emplace_back(entryPoint);

    shader->stages.emplace_back(SlangStageToVkStage(stage));
    stages.emplace_back(stage);

    PrintDebug(" - {}; index: {}", entryPointName,
               std::to_string(entryPoints.size() - 1));
  }

  std::vector<slang::IComponentType *> componentTypes;
  componentTypes.emplace_back(shader->slangModule);

  for (auto &entryPoint : entryPoints) {
    componentTypes.emplace_back(entryPoint);
  }

  Slang::ComPtr<slang::IComponentType> composedProgram;

  PrintDebug("Composing program...");
  result = session->createCompositeComponentType(
      componentTypes.data(), static_cast<SlangInt>(componentTypes.size()),
      composedProgram.writeRef(), diagnosticsBlob.writeRef());

  auto err = Error::Create(result, diagnosticsBlob, composedProgram.readRef());
  if (Error::IsError(err)) {
    return err;
  }

  PrintDebug("Linking program...");

  result = composedProgram->link(shader->linkedProgram.writeRef(),
                                 diagnosticsBlob.writeRef());

  err = Error::Create(result, diagnosticsBlob, shader->linkedProgram.readRef());
  if (Error::IsError(err)) {
    return err;
  }

  PrintDebug("Getting program layout...");

  shader->programLayout =
      shader->linkedProgram->getLayout(0, diagnosticsBlob.writeRef());

  err = Error::Create(result, diagnosticsBlob, shader->programLayout);
  if (Error::IsError(err)) {
    return err;
  }

  if (shader->stages.empty()) {
    return Error::Create("No valid entry points found in shader.");
  }

  if (shader->stages.at(0) == VK_SHADER_STAGE_COMPUTE_BIT) {
    if (entryPointCount != 1) {
      return Error::Create("Compute shader must have exactly one entry point.");
    }

    auto *entrypointReflection = shader->programLayout->getEntryPointByIndex(0);

    std::array<SlangUInt, 3> out_workgroupSize = {1, 1, 1};
    SlangUInt out_waveSize = 0;

    entrypointReflection->getComputeThreadGroupSize(3,
                                                    out_workgroupSize.data());
    entrypointReflection->getComputeWaveSize(&out_waveSize);

    shader->threadgroupSize =
        Math::Uvec3{static_cast<uint32_t>(out_workgroupSize[0]),
                    static_cast<uint32_t>(out_workgroupSize[1]),
                    static_cast<uint32_t>(out_workgroupSize[2])};
    shader->waveSize = static_cast<uint32_t>(out_waveSize);

    auto invocationlimit =
        context.deviceProperties.limits.maxComputeWorkGroupInvocations;

    Math::Uvec3 sizelimit{
        context.deviceProperties.limits.maxComputeWorkGroupSize[0],
        context.deviceProperties.limits.maxComputeWorkGroupSize[1],
        context.deviceProperties.limits.maxComputeWorkGroupSize[2],
    };

    if (out_workgroupSize[0] * out_workgroupSize[1] * out_workgroupSize[2] >
        invocationlimit) {
      return Error::Create("Compute shader threadgroup size exceeds device "
                           "limit of " +
                           std::to_string(invocationlimit) + " invocations.");
    }

    for (SlangUInt i = 0; i < 3; i++) {
      if (shader->threadgroupSize[i] > sizelimit[i]) {
        return Error::Create("Compute shader threadgroup size in dimension " +
                             std::to_string(i) + " exceeds device limit of " +
                             std::to_string(sizelimit[i]) + ".");
      }
    }
  }

  Slang::ComPtr<slang::IBlob> spirvCode;

  shader->stages.resize(entryPointCount);

  result = shader->linkedProgram->getTargetCode(0, // targetIndex
                                                spirvCode.writeRef(),
                                                diagnosticsBlob.writeRef());

  err = Error::Create(result, diagnosticsBlob, spirvCode.readRef());
  if (Error::IsError(err)) {
    return err;
  }

  PrintDebug("Creating Vulkan shader module...");

  std::vector<uint32_t> data;
  size_t codeSize = spirvCode->getBufferSize();
  data.resize(codeSize / 4);
  memcpy(data.data(), spirvCode->getBufferPointer(), codeSize);

  // NOLINTNEXTLINE
  std::span<uint8_t> spirvCodeSpan(reinterpret_cast<uint8_t *>(data.data()),
                                   codeSize);

  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = data.size() * sizeof(uint32_t);
  moduleCreateInfo.pCode = data.data();
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    Error error = Error::Create(
        vkCreateShaderModule(context.device, &moduleCreateInfo,
                             GetAllocationCallbacks(), &shader->module));

    if (Error::IsError(error)) {
      return error;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_SHADER_MODULE;

    // NOLINTNEXTLINE
    nameInfo.objectHandle = reinterpret_cast<uint64_t>(shader->module);
    nameInfo.pObjectName = shader->moduleName.c_str();

    vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
  }

#if Enable_Snapshots
  Snapshot::CaptureEvent(
      Snapshot::ShaderModuleCreateEvent(shader->module, shader->moduleName));
#endif

  PrintDebug("Shader module created successfully.");

  return Error::Success();
}

auto ShaderModule::Create(
    Graphics::GraphicsContext &context, const std::string &modulename,
    const std::string &name,
    const std::vector<slang::PreprocessorMacroDesc> *preprocessorMacros)
    -> Result<Ref<ShaderModule>> {
  Ref<ShaderModule> shader = Ref<ShaderModule>::Make();
  shader->name = name;
  shader->moduleName = modulename;

  if (preprocessorMacros != nullptr) {
    shader->preprocessorMacros = *preprocessorMacros;
  }

  switch (Window::GetWindowContext()->colorSpace) {
  case Window::ColorSpace::GammaCorrect:
    shader->preprocessorMacros.emplace_back("__COLORSPACE_GAMMA_CORRECT", "1");
    break;
  case Window::ColorSpace::Linear:
    shader->preprocessorMacros.emplace_back("__COLORSPACE_LINEAR", "1");
    break;
  case Window::ColorSpace::HDR:
    shader->preprocessorMacros.emplace_back("__COLORSPACE_HDR", "1");
    break;
  }

  // Compile Slang to SPIR-V and create shader module
  auto error = LoadSlang(context, shader);
  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  auto reflectResult =
      ReflectShader(context, shader->programLayout, shader->reflection);

  if (Error::IsError(reflectResult)) {
    return reflectResult.AsUnexpected();
  }

  for (auto &layout : shader->reflection.resources) {
    if (layout.IsBuffer()) {
      const auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

      if (bufferInfo.bufferType == Reflect::BufferType::PushConstant) {
        auto result = PushBuffer(layout);

        shader->pushBuffers.emplace_back(result);
      }
    }
  }

#if Enable_Snapshots
  Snapshot::CaptureEvent(
      Snapshot::ShaderModuleCreateEvent(shader->module, shader->moduleName));
#endif

  shader->globalUniforms.resize(
      shader->reflection.globalBufferFormat.GetStride());

  return shader;
}

inline auto ValidateBuffers(const ShaderModule *shader) -> Error {
  ZoneScoped;
  // Loop over shader->reflection.resources, and check if all buffers are
  // set up in shader->buffers,
  // this is done outside the shader as the user must manage these
  // resources themselves.

  for (const auto &resource : shader->reflection.resources) {
    if (!resource.IsBuffer()) {
      continue;
    }

    const auto &bufferInfo = std::get<Reflect::BufferInfo>(resource.info);

    // Not sent by the user
    if (bufferInfo.bufferType == Reflect::BufferType::PushConstant) {
      continue;
    }

    auto locationKey =
        Utils::SetBindingToSlot(bufferInfo.set, bufferInfo.binding);

    if (!shader->GetState().userBoundBuffers.contains(locationKey)) {
      return Error::Createf("Buffer '{}' not set up in shader.", resource.name);
    }
  }

  return Error::Success();
}

void append(ResourceKey &dest, const ResourceKey &src) {
  auto iterator = dest.before_begin();
  for (auto &&data : dest) {
    iterator++;
  }
  dest.insert_after(iterator, src.begin(), src.end());
}

auto ShaderModule::GetUniform(const ResourceKey &key) const
    -> const Reflect::ResourceInfo * {
  for (const auto &pushBuffer : pushBuffers) {
    const auto *info = pushBuffer.GetUniform(key.begin(), key.end());
    if (info == nullptr) {
      continue;
    }
    return info;
  }

  // check global ubo
  thread_local ResourceKey globalsKey;
  globalsKey.clear();
  globalsKey.emplace_front("Globals");
  append(globalsKey, key);

  const auto *info =
      reflection.globals.ResolvePath(globalsKey.begin(), globalsKey.end());
  if (info != nullptr) {
    return info;
  }

  for (const auto &resource : reflection.resources) {
    if (resource.name == *key.begin()) {
      return &resource;
    }
  }

  return nullptr;
}

auto ShaderModule::Send(const GraphicsContext &context, const ResourceKey &key,
                        const std::span<const uint8_t> &data) -> Error {
  ZoneScopedN("ShaderModule::Send data span");

  for (auto &pushBuffer : pushBuffers) {
    if (pushBuffer.ContainsUniform(key.begin(), key.end())) {
      return pushBuffer.SetData(key, data);
    }
  }

  // check global ubo
  thread_local ResourceKey globalsKey = {};
  globalsKey.clear();
  globalsKey.emplace_front("Globals");
  append(globalsKey, key);

  const auto *info =
      reflection.globals.ResolvePath(globalsKey.begin(), globalsKey.end());
  if (info == nullptr) {
    return Error::Create("Uniform `" + Reflect::ResourceKeyToString(key) +
                         "` not found.");
  }

  size_t offset = info->GetOffset();
  assert(offset + data.size() <= globalUniforms.size() &&
         "Data exceeds global uniform buffer size.");

  // NOLINTNEXTLINE, pointer arithmetic
  memcpy(globalUniforms.data() + offset, data.data(), data.size());

  return Error::Success();
}

auto ShaderModule::Send(const GraphicsContext &context, const ResourceKey &key,
                        const Ref<Buffer> &buffer) -> Error {
  ZoneScopedN("ShaderModule::Send structured buffer");

  if (!buffer.isValid()) {
    return Error::Create("Buffer is null.");
  }

  for (const auto &resource : reflection.resources) {
    if (!std::holds_alternative<Reflect::BufferInfo>(resource.info)) {
      continue;
    }

    const auto &bufferInfo = std::get<Reflect::BufferInfo>(resource.info);
    if (bufferInfo.name == *key.begin()) {
      auto locationKey =
          Utils::SetBindingToSlot(bufferInfo.set, bufferInfo.binding);

      GetState().userBoundBuffers[locationKey] =
          BoundBufferPair{buffer, &bufferInfo};

      return Error::Success();
    }
  }

  return Error::Create("Buffer not found in shader reflection: " + name);
}

auto ShaderModule::Send(const GraphicsContext &context, const ResourceKey &key,
                        const Ref<Graphics::Texture> &texture) -> Error {
  ZoneScopedN("ShaderModule::Send texture");

  if (!texture.isValid()) {
    return Error::Create("Texture is null.");
  }

  if (texture->view == VK_NULL_HANDLE) {
    return Error::Create("Texture has no valid image view.");
  }

  auto &state = GetState();

  for (const auto &resource : reflection.resources) {
    if (!std::holds_alternative<Reflect::SamplerInfo>(resource.info)) {
      continue;
    }

    const auto &samplerInfo = std::get<Reflect::SamplerInfo>(resource.info);
    if (resource.name == *key.begin()) { // TODO: Fix this search
      auto key = Utils::SetBindingToSlot(samplerInfo.set, samplerInfo.binding);

      if ((samplerInfo.access == SLANG_RESOURCE_ACCESS_WRITE ||
           samplerInfo.access == SLANG_RESOURCE_ACCESS_READ_WRITE) &&
          !texture->SupportsStorage()) {
        return Error::Create(
            "Texture does not support storage access required by shader.");
      }

      state.userBoundTextures[key] = {texture, &samplerInfo};

      return Error::Success();
    }
  }

  return Error::Create("Sampler not found in shader reflection: " + name);
}

auto ShaderModule::hash() const -> size_t {
  return reinterpret_cast<size_t>(module); // NOLINT
}

auto ShaderModule::GetThreadgroupSize() const -> Result<Math::Uvec3> {
  if (threadgroupSize.x == 0 || threadgroupSize.y == 0 ||
      threadgroupSize.z == 0) {
    return Error::Unexpected(
        "Shader is not a compute shader or threadgroup size not set.");
  }
  return threadgroupSize;
}

auto ShaderModule::GetWaveSize() const -> uint32_t { return waveSize; }

auto ShaderModule::GetSlotDescription(uint32_t set, uint32_t binding) // NOLINT
    -> const Reflect::ResourceInfo * {

  auto key = Utils::SetBindingToSlot(set, binding);

  auto iter = reflection.slotToInfo.find(key);
  if (iter == reflection.slotToInfo.end()) {
    return nullptr;
  }

  return &iter->second;
}

auto ShaderModule::GetSlotDescription(uint64_t slot)
    -> const Reflect::ResourceInfo * {

  auto iter = reflection.slotToInfo.find(slot);
  if (iter == reflection.slotToInfo.end()) {
    const auto &[set, binding] = Utils::SlotToSetBinding(slot);
    return nullptr;
  }

  return &iter->second;
}

} // namespace Graphics::Shader