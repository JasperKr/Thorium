#include "shader.hpp"
#include "Buffers/uniform.hpp"
#include "Graphics/Buffers/push.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/console.hpp"
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
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Graphics::Shader {

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
Ref<ShaderModule> DefaultShaderModule = {}; // NOLINT

auto LoadModule() -> Error::Error {
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
                             Ref<ShaderModule> &shader) -> Error::Error {
  slang::SessionDesc sessionDesc = {};
  sessionDesc.allowGLSLSyntax = false;
  sessionDesc.defaultMatrixLayoutMode =
      SlangMatrixLayoutMode::SLANG_MATRIX_LAYOUT_ROW_MAJOR;

  auto sourceAndSaveDirectory = std::string(".");
  auto shaderDirectory = Path::Join({"src", "Graphics", "Shaders"});

  std::vector<const char *> searchPaths = {sourceAndSaveDirectory.c_str(),
                                           shaderDirectory.c_str()};

  std::string directories = "Shader directories:\n";
  for (const auto &path : searchPaths) {
    directories += " - " + std::string(path) + "\n";
  }

  PrintDebug(directories);

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
  {
    PrintDebug("Composing program...");
    SlangResult result = session->createCompositeComponentType(
        componentTypes.data(), static_cast<SlangInt>(componentTypes.size()),
        composedProgram.writeRef(), diagnosticsBlob.writeRef());

    auto err =
        Error::Create(result, diagnosticsBlob, composedProgram.readRef());
    if (Error::IsError(err)) {
      return err;
    }
  }

  {
    PrintDebug("Linking program...");

    SlangResult result = composedProgram->link(shader->linkedProgram.writeRef(),
                                               diagnosticsBlob.writeRef());

    auto err =
        Error::Create(result, diagnosticsBlob, shader->linkedProgram.readRef());
    if (Error::IsError(err)) {
      return err;
    }
  }

  PrintDebug("Getting program layout...");

  shader->programLayout =
      shader->linkedProgram->getLayout(0, diagnosticsBlob.writeRef());

  auto err = Error::Create(result, diagnosticsBlob, shader->programLayout);
  if (Error::IsError(err)) {
    return err;
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

  auto fserr = Filesystem::CreateDirectory(SpirvDirectory);

  if (Error::IsError(fserr)) {
    return fserr;
  }

  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = data.size() * sizeof(uint32_t);
  moduleCreateInfo.pCode = data.data();
  Error::Error error = Error::Create(vkCreateShaderModule(
      context.device, &moduleCreateInfo, nullptr, &shader->module));

  if (Error::IsError(error)) {
    return error;
  }

  PrintDebug("Shader module created successfully.");

  return Error::Success();
}

auto ShaderModule::Create(Graphics::GraphicsContext &context,
                          const std::string &modulename,
                          const std::string &name)
    -> tl::expected<Ref<ShaderModule>, Error::Error> {
  Ref<ShaderModule> shader = Ref<ShaderModule>::Make();
  shader->name = name;
  shader->moduleName = modulename;

  // Compile Slang to SPIR-V and create shader module
  auto error = LoadSlang(context, shader);
  if (Error::IsError(error)) {
    return tl::unexpected(error);
  }

  auto reflectResult =
      ReflectShader(context, shader->programLayout, shader->reflection);

  if (Error::IsError(reflectResult)) {
    return tl::unexpected(reflectResult);
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

inline auto ValidateBuffers(const ShaderModule *shader) -> Error::Error {
  // Loop over shader->reflection.resources, and check if all buffers are
  // set up in shader->uniformBuffers and shader->storageBuffers,
  // this is done outside the shader as the user must manage these
  // resources themselves.

  for (const auto &resource : shader->reflection.resources) {
    if (!resource.IsBuffer()) {
      continue;
    }

    const auto &bufferInfo = std::get<BufferInfo>(resource.info);

    if (bufferInfo.bufferType == BufferType::Uniform) {
      if (!shader->uniformBuffers.contains(bufferInfo.set)) {
        return Error::Create("Uniform buffer '" + resource.name +
                             "' not set up in shader.");
      }
    } else if (bufferInfo.bufferType == BufferType::Storage) {
      if (!shader->storageBuffers.contains(bufferInfo.set)) {
        return Error::Create("Storage buffer '" + resource.name +
                             "' not set up in shader.");
      }
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
    -> tl::expected<const ResourceInfo, Error::Error> {
  for (const auto &pushBuffer : pushBuffers) {
    PrintDebug("Checking push buffer {} for key: {}...",
               pushBuffer.GetLayout().name, ResourceKeyToString(key));
    if (pushBuffer.ContainsUniform(key.begin(), key.end())) {
      const auto *const info =
          pushBuffer.GetLayout().ResolvePath(key.begin(), key.end());
      if (info == nullptr) {
        return tl::unexpected(
            Error::Create("Uniform not found in push buffer."));
      }
      return *info;
    }
  }

  // check global ubo
  ResourceKey globalsKey = {"Globals"};
  append(globalsKey, key);
  PrintDebug("Checking global UBO for key: {}...",
             ResourceKeyToString(globalsKey));

  const auto *info =
      reflection.globals.ResolvePath(globalsKey.begin(), globalsKey.end());
  if (info == nullptr) {
    return tl::unexpected(Error::Create("Uniform not found."));
  }

  return *info;
}

auto ShaderModule::Send(GraphicsContext &context, const ResourceKey &key,
                        const std::span<const uint8_t> &data) -> Error::Error {
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
  PrintDebug("Checking global UBO for key: {}...",
             ResourceKeyToString(globalsKey));

  const auto *info =
      reflection.globals.ResolvePath(globalsKey.begin(), globalsKey.end());
  if (info == nullptr) {
    return Error::Create("Uniform not found.");
  }

  size_t offset = info->GetOffset();
  if (offset + data.size() > globalUniforms.size()) {
    globalUniforms.resize(offset + data.size());
  }

  // NOLINTNEXTLINE, pointer arithmetic
  memcpy(globalUniforms.data() + offset, data.data(), data.size());

  return Error::Success();
}

auto ShaderModule::Send(GraphicsContext &context, const ResourceKey &key,
                        Buffer *buffer) -> Error::Error {

  for (const auto &resource : reflection.resources) {
    if (!std::holds_alternative<BufferInfo>(resource.info)) {
      continue;
    }

    const auto &bufferInfo = std::get<BufferInfo>(resource.info);
    if (bufferInfo.name == *key.begin()) {
      if (descriptorSets[bufferInfo.set] == VK_NULL_HANDLE) {
        return Error::Success(); // Will be created and set later
      }
      // NOLINTNEXTLINE
      auto key = bufferInfo.set | ((uint64_t)bufferInfo.binding << 32U);

      VkDescriptorBufferInfo vkBufferInfo{};
      vkBufferInfo.buffer = buffer->handle;
      vkBufferInfo.offset = 0;
      vkBufferInfo.range = buffer->size;

      DescriptorWriteInfo descriptorWrite{};
      descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet = bufferInfo.set;
      descriptorWrite.dstBinding = bufferInfo.binding;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pBufferInfo = vkBufferInfo;

      // vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
      pendingDescriptorWrites.emplace_back(descriptorWrite);

      return Error::Success();
    }
  }

  return Error::Create("Buffer not found in shader reflection: " + name);
}

auto ShaderModule::Send(GraphicsContext &context, const ResourceKey &key,
                        Graphics::Texture::Texture *texture) -> Error::Error {
  for (const auto &resource : reflection.resources) {
    if (!std::holds_alternative<SamplerInfo>(resource.info)) {
      continue;
    }

    const auto &samplerInfo = std::get<SamplerInfo>(resource.info);
    if (resource.name == *key.begin()) {
      // NOLINTNEXTLINE
      auto key = samplerInfo.set | ((uint64_t)samplerInfo.binding << 32U);

      // Create descriptor set for this texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView = texture->view;
      imageInfo.sampler = texture->GetSampler(context);

      DescriptorWriteInfo descriptorWrite{};
      descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet = samplerInfo.set;
      descriptorWrite.dstBinding = samplerInfo.binding;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType =
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pImageInfo = imageInfo;

      pendingDescriptorWrites.emplace_back(descriptorWrite);
      pendingImageTransitions.emplace_back(ImageTransitionInfo{
          .texture = texture,
          .newUsage = Texture::TextureUsage::Sampler,
          .newStage = ShaderStageFlagsToPipelineStageFlags(resource.stages),
      });

      return Error::Success();
    }
  }

  return Error::Create("Sampler not found in shader reflection: " + name);
}

auto ShaderModule::hash() const -> size_t {
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

auto ShaderModule::FlushBuffers(GraphicsContext &context,
                                VkPipelineLayout layout) -> Error::Error {
  auto validateResult = ValidateBuffers(this);
  if (Error::IsError(validateResult)) {
    return validateResult;
  }

  static Graphics::Buffer *currentUBOBuffer;

  auto &buffer = GetGlobalUniformBuffer(context.frameIndex);
  buffer.SetData(context, globalUniforms, 0);
  auto uboFlushResult = buffer.Flush(context);

  if (Error::IsError(uboFlushResult)) {
    return uboFlushResult.error();
  }

  {
    // UBO buffer can be resized, we update every frame for now;
    VkDescriptorBufferInfo bufferInfo{};

    bufferInfo.buffer = buffer.GetBuffer().get()->handle;
    bufferInfo.offset = buffer.GetOffset() - buffer.GetLastFlushSize();

    assert(reflection.globals.size > 0);

    bufferInfo.range = buffer.GetLastFlushSize();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSets[reflection.globals.set];
    descriptorWrite.dstBinding = reflection.globals.binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);

    // currentUBOBuffer = buffer.GetBuffer().get();
  }

  std::vector<VkWriteDescriptorSet> writes;
  std::set<uint64_t> updatedSets;

  auto writeCount = static_cast<int32_t>(pendingDescriptorWrites.size());
  writes.reserve(writeCount);

  // Loop over writes in reverse to prioritize later writes
  for (int32_t i = writeCount - 1; i >= 0; i--) {
    auto &write = pendingDescriptorWrites.at(i);
    uint64_t key = write.dstSet;
    key |= (static_cast<uint64_t>(write.dstBinding) << 32U); // NOLINT
    if (updatedSets.contains(key)) {
      continue;
    }
    updatedSets.insert(key);

    writes.emplace_back(write.GetWrite(descriptorSets));
  }

  for (auto &transition : pendingImageTransitions) {
    Error::Error result;

    switch (transition.newUsage) {
    case Texture::TextureUsage::Sampler:
      result = transition.texture->UseAsSampler(context, transition.newStage);
      break;
    case Texture::TextureUsage::Storage:
      result = transition.texture->UseAsStorage(context, transition.newStage);
      break;
    case Texture::TextureUsage::Attachment:
      result = transition.texture->UseAsAttachment(context);
      break;
    case Texture::TextureUsage::TransferSrc:
      result = transition.texture->UseAsTransferSrc(context);
      break;
    case Texture::TextureUsage::TransferDst:
      result = transition.texture->UseAsTransferDst(context);
      break;
    case Texture::TextureUsage::Unknown:
      result = Error::Create(
          "Cannot transition image with unknown usage in shader flush.");
      break;
    }

    if (Error::IsError(result)) {
      return result;
    }
  }
  pendingImageTransitions.clear();

  vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);
  pendingDescriptorWrites.clear();

  auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

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

  std::vector<VkDescriptorSet> descriptorSetList;
  descriptorSetList.reserve(this->descriptorSets.size());
  uint32_t set = 0;
  for (const auto &setPair : this->descriptorSets) {
    // I assume slang will always output sets in order
    // Unless the user manually assigns set numbers out of order
    // In that case, don't, lol
    assert(setPair.first == set && "Descriptor sets must be bound in order.");
    descriptorSetList.emplace_back(setPair.second);
  }

  vkCmdBindDescriptorSets(commandBuffer, bindpoint, layout, 0,
                          static_cast<uint32_t>(descriptorSetList.size()),
                          descriptorSetList.data(), 0, nullptr);

  for (auto &pushBuffer : pushBuffers) {
    FlushInfo info{
        .commandBuffer = commandBuffer,
        .pipelineLayout = layout,
    };

    pushBuffer.FlushData(info);
  }

  return Error::Success();
}

void ShaderModule::Destroy(VkDevice &device) {
  for (auto &pair : descriptorSetLayouts) {
    vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
  }

  if (module != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device, module, nullptr);
    module = VK_NULL_HANDLE;
  }
}

} // namespace Graphics::Shader