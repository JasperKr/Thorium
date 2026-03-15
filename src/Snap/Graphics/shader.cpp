#include "shader.hpp"
#include "Buffers/uniform.hpp"
#include "Graphics/Buffers/push.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/texture.hpp"
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
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string_view>
#include <unordered_set>
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
thread_local std::unordered_map<const struct ShaderModule *, BoundState>
    BoundStates;
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
    auto entryPointName = SlangStageToString(stage) + "Main";

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
    Error error = Error::Create(vkCreateShaderModule(
        context.device, &moduleCreateInfo, nullptr, &shader->module));

    if (Error::IsError(error)) {
      return error;
    }
  }

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
      auto &bufferInfo = std::get<BufferInfo>(layout.info);

      if (bufferInfo.bufferType == BufferType::PushConstant) {
        auto result = PushBuffer(layout);

        shader->pushBuffers.emplace_back(result);
      }
    }
  }

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

    const auto &bufferInfo = std::get<BufferInfo>(resource.info);

    // Not sent by the user
    if (bufferInfo.bufferType == BufferType::PushConstant) {
      continue;
    }

    auto locationKey = SetBindingToSlot(bufferInfo.set, bufferInfo.binding);

    if (!shader->GetState().boundBuffers.contains(locationKey)) {
      return Error::Create("Storage buffer '" + resource.name +
                           "' not set up in shader.");
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
    -> Result<const ResourceInfo> {
  for (const auto &pushBuffer : pushBuffers) {
    const auto *const info = pushBuffer.GetUniform(key.begin(), key.end());
    if (info == nullptr) {
      continue;
    }
    return *info;
  }

  // check global ubo
  ResourceKey globalsKey = {"Globals"};
  append(globalsKey, key);

  const auto *info =
      reflection.globals.ResolvePath(globalsKey.begin(), globalsKey.end());
  if (info == nullptr) {
    return Error::Unexpected("Uniform not found.");
  }

  return *info;
}

auto ShaderModule::Send(const GraphicsContext &context, const ResourceKey &key,
                        const std::span<const uint8_t> &data) -> Error {
  ZoneScopedN("ShaderModule::Send data span");

  for (auto &pushBuffer : pushBuffers) {
    PrintDebug("Checking push buffer {} for key: {}...",
               pushBuffer.GetLayout().name, ResourceKeyToString(key));
    if (pushBuffer.ContainsUniform(key.begin(), key.end())) {
      return pushBuffer.SetData(key, data);
    }
  }

  // check global ubo
  ResourceKey globalsKey = {"Globals"};
  append(globalsKey, key);

  const auto *info =
      reflection.globals.ResolvePath(globalsKey.begin(), globalsKey.end());
  if (info == nullptr) {
    return Error::Create("Uniform `" + ResourceKeyToString(key) +
                         "` not found.");
  }

  size_t offset = info->GetOffset();
  if (offset + data.size() > globalUniforms.size()) {
    globalUniforms.resize(offset + data.size());
  }

  // NOLINTNEXTLINE, pointer arithmetic
  memcpy(globalUniforms.data() + offset, data.data(), data.size());

  return Error::Success();
}

auto ShaderModule::Send(const GraphicsContext &context, const ResourceKey &key,
                        StructuredBuffer *buffer) -> Error {
  ZoneScopedN("ShaderModule::Send structured buffer");

  if (buffer == nullptr) {
    return Error::Create("Buffer is null.");
  }

  for (const auto &resource : reflection.resources) {
    if (!std::holds_alternative<BufferInfo>(resource.info)) {
      continue;
    }

    const auto &bufferInfo = std::get<BufferInfo>(resource.info);
    if (bufferInfo.name == *key.begin()) {
      auto locationKey = SetBindingToSlot(bufferInfo.set, bufferInfo.binding);

      auto *bufferHandle = buffer->GetBuffer().get();

      GetState().boundBuffers[locationKey] = bufferHandle;

      VkDescriptorBufferInfo vkBufferInfo{};
      vkBufferInfo.buffer = bufferHandle->handle;
      vkBufferInfo.offset = 0;
      vkBufferInfo.range = bufferHandle->size;

      DescriptorWriteInfo descriptorWrite{};
      descriptorWrite.dstSet = bufferInfo.set;
      descriptorWrite.dstBinding = bufferInfo.binding;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType =
          (bufferInfo.bufferType == BufferType::Uniform
               ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
               : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pBufferInfo = vkBufferInfo;
      descriptorWrite.bufferPtr = bufferHandle;

      switch (bufferInfo.access) {
      case SLANG_RESOURCE_ACCESS_READ:
        descriptorWrite.bufferAccessBits = VK_ACCESS_2_SHADER_READ_BIT;
        break;
      case SLANG_RESOURCE_ACCESS_WRITE:
        descriptorWrite.bufferAccessBits = VK_ACCESS_2_SHADER_WRITE_BIT;
        break;
      case SLANG_RESOURCE_ACCESS_READ_WRITE:
        descriptorWrite.bufferAccessBits = static_cast<VkAccessFlagBits2>(
            VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
        break;
      default:
        descriptorWrite.bufferAccessBits = VK_ACCESS_2_SHADER_READ_BIT;
        break;
      };

      GetState().pendingDescriptorWrites.emplace_back(descriptorWrite);

      return Error::Success();
    }
  }

  return Error::Create("Buffer not found in shader reflection: " + name);
}

auto ShaderModule::Send(const GraphicsContext &context, const ResourceKey &key,
                        Graphics::Texture::Texture *texture) -> Error {
  ZoneScopedN("ShaderModule::Send texture");

  if (texture == nullptr) {
    return Error::Create("Texture is null.");
  }

  if (texture->view == VK_NULL_HANDLE) {
    return Error::Create("Texture has no valid image view.");
  }

  auto &state = GetState();

  for (const auto &resource : reflection.resources) {
    if (!std::holds_alternative<SamplerInfo>(resource.info)) {
      continue;
    }

    const auto &samplerInfo = std::get<SamplerInfo>(resource.info);
    if (resource.name == *key.begin()) { // TODO: Fix this search
      auto key = SetBindingToSlot(samplerInfo.set, samplerInfo.binding);

      if ((samplerInfo.access == SLANG_RESOURCE_ACCESS_WRITE ||
           samplerInfo.access == SLANG_RESOURCE_ACCESS_READ_WRITE) &&
          !texture->SupportsStorage()) {
        return Error::Create(
            "Texture does not support storage access required by shader.");
      }

      state.userBoundTextures[key] = {texture, samplerInfo};

      return Error::Success();
    }
  }

  return Error::Create("Sampler not found in shader reflection: " + name);
}

auto ShaderModule::hash() const -> size_t {
  return reinterpret_cast<size_t>(module); // NOLINT
}

auto ShaderModule::FlushGlobals(const GraphicsContext &context,
                                VkPipelineLayout layout,
                                VkPipelineStageFlags2 dstStage) -> Error {
  ZoneScoped;

  auto &buffer = GetGlobalUniformBuffer(context.frameIndex);
  buffer.SetData(context, globalUniforms, 0);
  auto uboFlushResult = buffer.Flush(context);

  if (Error::IsError(uboFlushResult)) {
    return uboFlushResult.error();
  }

  if (reflection.hasGlobals) {
    // UBO buffer can be resized, we update every frame for now;
    // TODO: dynamic UBO offsets using VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    // and only update size when a draw requires more space.
    VkDescriptorBufferInfo bufferInfo{};

    bufferInfo.buffer = buffer.GetBuffer()->handle;
    bufferInfo.offset = buffer.GetOffset() - buffer.GetLastFlushSize();

    assert(reflection.globals.size > 0);

    bufferInfo.range = buffer.GetLastFlushSize();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    auto dstSetIter = GetState().descriptorSets.find(reflection.globals.set);
    if (dstSetIter != GetState().descriptorSets.end()) {
      descriptorWrite.dstSet = dstSetIter->second;
    } else {
      return Error::Createf(
          "Global UBO descriptor set {} not found in shader descriptor sets.",
          reflection.globals.set);
    }

    descriptorWrite.dstBinding = reflection.globals.binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
    }
  }

  return Error::Success();
}

// NOLINTNEXTLINE
auto ShaderModule::FlushDescriptors(const GraphicsContext &context,
                                    VkPipelineLayout layout,
                                    VkPipelineStageFlags2 dstStage) -> Error {
  ZoneScoped;

  auto validateResult = ValidateBuffers(this);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }

  static Graphics::Buffer *currentUBOBuffer;

  auto updateResult = FlushGlobals(context, layout, dstStage);
  if (Error::IsError(updateResult)) {
    return updateResult;
  }

  auto &state = GetState();

  thread_local std::vector<VkWriteDescriptorSet> writes;
  thread_local std::unordered_set<uint64_t> updatedSets;
  thread_local std::vector<VkDescriptorImageInfo> imageInfos;
  writes.clear();
  updatedSets.clear();
  imageInfos.clear();

  for (auto &resource : reflection.resources) {
    if (!resource.IsSampler()) {
      continue;
    }

    const auto &samplerInfo = std::get<SamplerInfo>(resource.info);
    auto usage = samplerInfo.access == SLANG_RESOURCE_ACCESS_READ
                     ? Texture::TextureUsage::Sampler
                     : Texture::TextureUsage::Storage;
    auto stage = ShaderStageFlagsToPipelineStageFlags(resource.stages);

    auto key = SetBindingToSlot(samplerInfo.set, samplerInfo.binding);
    auto boundTextureIter = state.userBoundTextures.find(key);
    if (boundTextureIter == state.userBoundTextures.end()) {
      PrintWarning("No texture bound for sampler '{}', set {}, binding {}.",
                   resource.name, samplerInfo.set, samplerInfo.binding);
      continue;
    }
    auto *texture = boundTextureIter->second.first;

    if (texture == nullptr) {
      PrintWarning("Texture bound to sampler '{}' is null, set {}, binding {}.",
                   resource.name, samplerInfo.set, samplerInfo.binding);
      continue;
    }

    auto currentlyBound = state.boundTextures.contains(key);
    if (currentlyBound && state.boundTextures.at(key) == texture) {
      continue;
    }

    // PrintAlways(
    //     "Flushing sampler resource '{}' with usage {}...", resource.name,
    //     usage == Texture::TextureUsage::Sampler ? "sampler" : "storage");

    Error result;

    switch (usage) {
    case Texture::TextureUsage::Sampler:
      result = texture->UseAsSampler(context, stage);
      break;
    case Texture::TextureUsage::Storage:
      result = texture->UseAsStorage(context, stage);
      break;
    case Texture::TextureUsage::Attachment:
      result = texture->UseAsAttachment(context);
      break;
    case Texture::TextureUsage::TransferSrc:
      result = texture->UseAsTransferSrc(context);
      break;
    case Texture::TextureUsage::TransferDst:
      result = texture->UseAsTransferDst(context);
      break;
    case Texture::TextureUsage::PresentSrc:
      result = texture->UseAsPresentSrc(context);
      break;
    case Texture::TextureUsage::Unknown:
      result = Error::Create(
          "Cannot transition image with unknown usage in shader flush.");
      break;
    }

    if (Error::IsError(result)) {
      return result;
    }

    imageInfos.emplace_back();
    auto &imageInfo = imageInfos.back();

    imageInfo.imageView = texture->view;
    imageInfo.imageLayout = texture->currentLayout;
    imageInfo.sampler = texture->GetSampler(context);

    VkWriteDescriptorSet descSetWrite{};
    descSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descSetWrite.pNext = nullptr;
    descSetWrite.dstSet = state.descriptorSets.at(samplerInfo.set);
    descSetWrite.dstBinding = samplerInfo.binding;
    descSetWrite.dstArrayElement = 0;
    descSetWrite.descriptorCount = 1;
    descSetWrite.descriptorType =
        (usage == Texture::TextureUsage::Storage
             ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
             : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descSetWrite.pImageInfo = &imageInfo;
    descSetWrite.pBufferInfo = nullptr;
    descSetWrite.pTexelBufferView = nullptr;

    writes.emplace_back(descSetWrite);
    state.boundTextures[key] = texture;
  }

  {
    ZoneScopedN("Update descriptor sets");
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkUpdateDescriptorSets(context.device, writes.size(), writes.data(), 0,
                           nullptr);
  }
  state.pendingDescriptorWrites.clear();

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == VK_NULL_HANDLE) {
    return Error::Create("No valid command buffer found for shader flush.");
  }

  VkPipelineBindPoint bindpoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  for (auto &stage : stages) {
    if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
      bindpoint = VK_PIPELINE_BIND_POINT_COMPUTE;
      break;
    }
    if (stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR) {
      bindpoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
      break;
    }
  }

  thread_local std::vector<VkDescriptorSet> descriptorSetList;
  descriptorSetList.clear();
  descriptorSetList.reserve(state.descriptorSets.size());
  uint32_t set = 0;
  for (const auto &setPair : state.descriptorSets) {
    // I assume slang will always output sets in order
    // Unless the user manually assigns set numbers out of order
    // In that case, don't, lol
    assert(setPair.first == set && "Descriptor sets must be bound in order.");
    descriptorSetList.emplace_back(setPair.second);
  }

  {
    ZoneScopedN("Bind descriptor sets")
        vkCmdBindDescriptorSets(commandBuffer, bindpoint, layout, 0,
                                static_cast<uint32_t>(descriptorSetList.size()),
                                descriptorSetList.data(), 0, nullptr);
  }

  return Error::Success();
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
    -> Result<const ResourceInfo> {

  auto key = SetBindingToSlot(set, binding);

  auto iter = reflection.slotToInfo.find(key);
  if (iter == reflection.slotToInfo.end()) {
    return Error::Unexpectedf(
        "Slot (set: {}, binding: {}) not found in shader reflection.", set,
        binding);
  }

  return iter->second;
}

auto ShaderModule::GetSlotDescription(uint64_t slot)
    -> Result<const ResourceInfo> {

  auto iter = reflection.slotToInfo.find(slot);
  if (iter == reflection.slotToInfo.end()) {
    const auto &[set, binding] = SlotToSetBinding(slot);
    return Error::Unexpectedf(
        "Slot (set: {}, binding: {}) not found in shader reflection.", set,
        binding);
  }

  return iter->second;
}

} // namespace Graphics::Shader