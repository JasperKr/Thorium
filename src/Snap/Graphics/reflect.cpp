#include "reflect.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"

#include "vulkan/vulkan_core.h"

// Resolve path for struct fields is exclusive to the struct resource info
// Since structs are nameless and their name is only described by the parent resource info
auto StructInfo::ResolvePath(ResourceKey::const_iterator iterator,
                             ResourceKey::const_iterator end) const
    -> const ResourceInfo * {
  const ResourceInfo *field = nullptr;

  for (const auto &currentField : fields) {
    if (currentField.name == *iterator) {
      field = &currentField;
      break;
    }
  }

  if (field == nullptr) {
    return nullptr;
  }

  if (std::next(iterator) != end) {
    if (!std::holds_alternative<StructInfo>(field->info)) {
      return field;
    }

    return field->ResolvePath(std::next(iterator), end);
  }

  return field;
}

auto ResourceInfo::ResolvePath(ResourceKey::const_iterator iterator,
                               ResourceKey::const_iterator end) const
    -> const ResourceInfo * {
  // Last element and name does not match, return nullptr
  if (iterator == end || *iterator != name) {
    return nullptr;
  }

  // Last element, and name matches, return this
  if (std::next(iterator) == end) {
    return this;
  }

  // Not a struct, cannot resolve further
  if (!std::holds_alternative<StructInfo>(info)) {
    return nullptr;
  }

  const auto &structInfo = std::get<StructInfo>(info);
  return structInfo.ResolvePath(std::next(iterator), end);
}

auto BufferInfo::ResolvePath(ResourceKey::const_iterator iterator,
                             ResourceKey::const_iterator end) const
    -> const ResourceInfo * {
  if (std::next(iterator) == end) {
    return nullptr;
  }

  if (!std::holds_alternative<StructInfo>(info)) {
    return nullptr;
  }

  const auto &structInfo = std::get<StructInfo>(info);
  return structInfo.ResolvePath(std::next(iterator), end);
}

auto SetupStruct(slang::TypeLayoutReflection *bufferLayout, // NOLINT
                 slang::TypeLayoutReflection *typeLayout)
    -> Result<std::variant<StructInfo, ScalarInfo, VectorInfo, MatrixInfo>> {
  switch (bufferLayout->getKind()) {
  case slang::TypeReflection::Kind::Struct: {
    auto structInfo = StructInfo{};

    structInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
    structInfo.alignment = static_cast<uint32_t>(bufferLayout->getAlignment());

    auto fieldCount = bufferLayout->getFieldCount();

    for (int i = 0; i < fieldCount; ++i) {
      auto *fieldVariableType = bufferLayout->getFieldByIndex(i);
      auto *fieldType = fieldVariableType->getTypeLayout();

      switch (fieldType->getKind()) {
      case slang::TypeReflection::Kind::Scalar: {
        auto scalarInfo = ScalarInfo{};

        scalarInfo.size = static_cast<uint32_t>(fieldType->getSize());
        scalarInfo.offset =
            static_cast<uint32_t>(fieldVariableType->getOffset());
        scalarInfo.type = FromScalarType(fieldType->getScalarType());
        ResourceInfo fieldInfo{
            .name = fieldVariableType->getName(),
            .info = scalarInfo,
        };

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Vector: {
        auto vectorInfo = VectorInfo{};

        vectorInfo.size = static_cast<uint32_t>(fieldType->getSize());
        vectorInfo.offset =
            static_cast<uint32_t>(fieldVariableType->getOffset());
        vectorInfo.scalarType = FromScalarType(fieldType->getScalarType());
        vectorInfo.vectorType = ToVectorType(fieldType->getElementCount());

        ResourceInfo fieldInfo{
            .name = fieldVariableType->getName(),
            .info = vectorInfo,
        };

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Matrix: {
        auto matrixInfo = MatrixInfo{};

        matrixInfo.size = static_cast<uint32_t>(fieldType->getSize());
        matrixInfo.offset =
            static_cast<uint32_t>(fieldVariableType->getOffset());
        matrixInfo.matrixType =
            ToMatrixType(fieldType->getRowCount(), fieldType->getColumnCount());

        ResourceInfo fieldInfo{
            .name = fieldVariableType->getName(),
            .info = matrixInfo,
        };

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Struct: {
        auto result = SetupStruct(fieldType, fieldType);
        if (Error::IsError(result)) {
          return result.error().AsUnexpected();
        }

        auto structFieldInfo = result.value();

        ResourceInfo fieldInfo{
            .name = fieldVariableType->getName(),
        };

        if (std::holds_alternative<StructInfo>(structFieldInfo)) {
          fieldInfo.info = std::get<StructInfo>(structFieldInfo);
        } else if (std::holds_alternative<ScalarInfo>(structFieldInfo)) {
          fieldInfo.info = std::get<ScalarInfo>(structFieldInfo);
        } else if (std::holds_alternative<VectorInfo>(structFieldInfo)) {
          fieldInfo.info = std::get<VectorInfo>(structFieldInfo);
        } else if (std::holds_alternative<MatrixInfo>(structFieldInfo)) {
          fieldInfo.info = std::get<MatrixInfo>(structFieldInfo);
        } else {
          return Error::Create("Unsupported struct field type in nested struct "
                               "reflection.")
              .AsUnexpected();
        }

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      default: {
        return Error::Unexpectedf(
            "Unsupported struct field type in Buffer struct reflection.");
      }
      }
    }

    return structInfo;
  }
  case slang::TypeReflection::Kind::Scalar: {
    auto scalarInfo = ScalarInfo{};
    scalarInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
    scalarInfo.offset =
        static_cast<uint32_t>(typeLayout->getElementVarLayout()->getOffset());
    scalarInfo.type = FromScalarType(bufferLayout->getScalarType());

    return scalarInfo;
  }
  case slang::TypeReflection::Kind::Vector: {
    auto vectorInfo = VectorInfo{};

    vectorInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
    vectorInfo.offset =
        static_cast<uint32_t>(typeLayout->getElementVarLayout()->getOffset());
    vectorInfo.scalarType = FromScalarType(bufferLayout->getScalarType());
    vectorInfo.vectorType = ToVectorType(bufferLayout->getElementCount());

    return vectorInfo;
  }
  case slang::TypeReflection::Kind::Matrix: {
    auto matrixInfo = MatrixInfo{};

    matrixInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
    matrixInfo.offset =
        static_cast<uint32_t>(typeLayout->getElementVarLayout()->getOffset());
    matrixInfo.matrixType = ToMatrixType(bufferLayout->getRowCount(),
                                         bufferLayout->getColumnCount());

    return matrixInfo;
  }
  default: {
    return Error::Unexpectedf(
        "Unsupported buffer element type in Buffer reflection.");
  }
  }

  int kind = static_cast<int>(bufferLayout->getKind());

  return Error::Unexpectedf(
      "Unsupported type layout kind for Buffer reflection: {}", kind);
}

inline auto SlangImageFormatToVkFormat(SlangImageFormat format) {
  switch (format) {
  case SLANG_IMAGE_FORMAT_unknown:
    return VK_FORMAT_UNDEFINED;
  case SLANG_IMAGE_FORMAT_rgba32f:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case SLANG_IMAGE_FORMAT_rgba16f:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case SLANG_IMAGE_FORMAT_rg32f:
    return VK_FORMAT_R32G32_SFLOAT;
  case SLANG_IMAGE_FORMAT_rg16f:
    return VK_FORMAT_R16G16_SFLOAT;
  case SLANG_IMAGE_FORMAT_r11f_g11f_b10f:
    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
  case SLANG_IMAGE_FORMAT_r32f:
    return VK_FORMAT_R32_SFLOAT;
  case SLANG_IMAGE_FORMAT_r16f:
    return VK_FORMAT_R16_SFLOAT;
  case SLANG_IMAGE_FORMAT_rgba16:
    return VK_FORMAT_R16G16B16A16_UNORM;
  case SLANG_IMAGE_FORMAT_rgb10_a2:
    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  case SLANG_IMAGE_FORMAT_rgba8:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case SLANG_IMAGE_FORMAT_rg16:
    return VK_FORMAT_R16G16_UNORM;
  case SLANG_IMAGE_FORMAT_rg8:
    return VK_FORMAT_R8G8_UNORM;
  case SLANG_IMAGE_FORMAT_r16:
    return VK_FORMAT_R16_UNORM;
  case SLANG_IMAGE_FORMAT_r8:
    return VK_FORMAT_R8_UNORM;
  case SLANG_IMAGE_FORMAT_rgba16_snorm:
    return VK_FORMAT_R16G16B16A16_SNORM;
  case SLANG_IMAGE_FORMAT_rgba8_snorm:
    return VK_FORMAT_R8G8B8A8_SNORM;
  case SLANG_IMAGE_FORMAT_rg16_snorm:
    return VK_FORMAT_R16G16_SNORM;
  case SLANG_IMAGE_FORMAT_rg8_snorm:
    return VK_FORMAT_R8G8_SNORM;
  case SLANG_IMAGE_FORMAT_r16_snorm:
    return VK_FORMAT_R16_SNORM;
  case SLANG_IMAGE_FORMAT_r8_snorm:
    return VK_FORMAT_R8_SNORM;
  case SLANG_IMAGE_FORMAT_rgba32i:
    return VK_FORMAT_R32G32B32A32_SINT;
  case SLANG_IMAGE_FORMAT_rgba16i:
    return VK_FORMAT_R16G16B16A16_SINT;
  case SLANG_IMAGE_FORMAT_rgba8i:
    return VK_FORMAT_R8G8B8A8_SINT;
  case SLANG_IMAGE_FORMAT_rg32i:
    return VK_FORMAT_R32G32_SINT;
  case SLANG_IMAGE_FORMAT_rg16i:
    return VK_FORMAT_R16G16_SINT;
  case SLANG_IMAGE_FORMAT_rg8i:
    return VK_FORMAT_R8G8_SINT;
  case SLANG_IMAGE_FORMAT_r32i:
    return VK_FORMAT_R32_SINT;
  case SLANG_IMAGE_FORMAT_r16i:
    return VK_FORMAT_R16_SINT;
  case SLANG_IMAGE_FORMAT_r8i:
    return VK_FORMAT_R8_SINT;
  case SLANG_IMAGE_FORMAT_rgba32ui:
    return VK_FORMAT_R32G32B32A32_UINT;
  case SLANG_IMAGE_FORMAT_rgba16ui:
    return VK_FORMAT_R16G16B16A16_UINT;
  case SLANG_IMAGE_FORMAT_rgb10_a2ui:
    return VK_FORMAT_A2R10G10B10_UINT_PACK32;
  case SLANG_IMAGE_FORMAT_rgba8ui:
    return VK_FORMAT_R8G8B8A8_UINT;
  case SLANG_IMAGE_FORMAT_rg32ui:
    return VK_FORMAT_R32G32_UINT;
  case SLANG_IMAGE_FORMAT_rg16ui:
    return VK_FORMAT_R16G16_UINT;
  case SLANG_IMAGE_FORMAT_rg8ui:
    return VK_FORMAT_R8G8_UINT;
  case SLANG_IMAGE_FORMAT_r32ui:
    return VK_FORMAT_R32_UINT;
  case SLANG_IMAGE_FORMAT_r16ui:
    return VK_FORMAT_R16_UINT;
  case SLANG_IMAGE_FORMAT_r8ui:
    return VK_FORMAT_R8_UINT;
  case SLANG_IMAGE_FORMAT_r64ui:
    return VK_FORMAT_R64_UINT;
  case SLANG_IMAGE_FORMAT_r64i:
    return VK_FORMAT_R64_SINT;
  case SLANG_IMAGE_FORMAT_bgra8:
    return VK_FORMAT_B8G8R8A8_UNORM;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

inline auto SlangStageToVkStage(SlangStage stage) -> VkShaderStageFlags {
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

auto SetupResource(slang::VariableLayoutReflection *variableLayout,
                   ShaderReflection &reflection) -> Error {

  auto *typeLayout = variableLayout->getTypeLayout();
  auto kind = typeLayout->getKind();
  auto shape = typeLayout->getResourceShape();
  auto access = typeLayout->getResourceAccess();

  // Masked out shape flags
  auto maskedShape = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;

  if (maskedShape == SLANG_TEXTURE_1D || maskedShape == SLANG_TEXTURE_2D ||
      maskedShape == SLANG_TEXTURE_3D || maskedShape == SLANG_TEXTURE_CUBE ||
      maskedShape == SLANG_TEXTURE_BUFFER) {

    auto samplerInfo = SamplerInfo{};
    samplerInfo.set = variableLayout->getBindingSpace();
    samplerInfo.binding = variableLayout->getBindingIndex();
    samplerInfo.shape = shape;
    samplerInfo.access = access;
    samplerInfo.format =
        SlangImageFormatToVkFormat(variableLayout->getImageFormat());

    auto resourceInfo = ResourceInfo{};

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = samplerInfo;

    reflection.resources.emplace_back(resourceInfo);
    reflection
        .slotToInfo[SetBindingToSlot(samplerInfo.set, samplerInfo.binding)] =
        resourceInfo;
  } else if (maskedShape == SLANG_STRUCTURED_BUFFER ||
             maskedShape == SLANG_BYTE_ADDRESS_BUFFER) {
    // SSBO
    auto bufferInfo = BufferInfo{};
    bufferInfo.name = variableLayout->getName();
    bufferInfo.set = variableLayout->getBindingSpace();
    bufferInfo.binding = variableLayout->getBindingIndex();
    bufferInfo.access = access;
    bufferInfo.bufferType = BufferType::Storage;

    auto *bufferLayout =
        variableLayout->getTypeLayout()->getElementTypeLayout();

    switch (bufferLayout->getKind()) {
    case slang::TypeReflection::Kind::Struct: {
      auto structInfo = StructInfo{};

      structInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
      structInfo.alignment =
          static_cast<uint32_t>(bufferLayout->getAlignment());
      auto fieldCount = bufferLayout->getFieldCount();

      for (int i = 0; i < fieldCount; ++i) {
        auto *fieldVariableType = bufferLayout->getFieldByIndex(i);
        auto *fieldType = fieldVariableType->getType();

        switch (fieldType->getKind()) {
        case slang::TypeReflection::Kind::Scalar: {
          auto scalarInfo = ScalarInfo{};

          scalarInfo.size = sizeof(float);
          scalarInfo.offset =
              static_cast<uint32_t>(fieldVariableType->getOffset());
          scalarInfo.type = FromScalarType(fieldType->getScalarType());
          ResourceInfo fieldInfo{
              .name = fieldVariableType->getName(),
              .info = scalarInfo,
          };

          structInfo.fields.emplace_back(fieldInfo);

          break;
        }
        case slang::TypeReflection::Kind::Vector: {
          auto vectorInfo = VectorInfo{};

          vectorInfo.size = static_cast<uint32_t>(sizeof(float) *
                                                  fieldType->getElementCount());
          vectorInfo.offset =
              static_cast<uint32_t>(fieldVariableType->getOffset());
          vectorInfo.scalarType = FromScalarType(fieldType->getScalarType());
          vectorInfo.vectorType = ToVectorType(fieldType->getElementCount());

          ResourceInfo fieldInfo{
              .name = fieldVariableType->getName(),
              .info = vectorInfo,
          };

          structInfo.fields.emplace_back(fieldInfo);

          break;
        }
        case slang::TypeReflection::Kind::Matrix: {
          auto matrixInfo = MatrixInfo{};

          matrixInfo.size =
              static_cast<uint32_t>(sizeof(float) * fieldType->getRowCount() *
                                    fieldType->getColumnCount());
          matrixInfo.offset =
              static_cast<uint32_t>(fieldVariableType->getOffset());
          matrixInfo.matrixType = ToMatrixType(fieldType->getRowCount(),
                                               fieldType->getColumnCount());

          ResourceInfo fieldInfo{
              .name = fieldVariableType->getName(),
              .info = matrixInfo,
          };

          structInfo.fields.emplace_back(fieldInfo);
          break;
        }
        default: {
          return Error::Create(
              "Unsupported struct field type in SSBO struct reflection.");
        }
        }
      }

      bufferInfo.info = structInfo;

      break;
    }
    case slang::TypeReflection::Kind::Scalar: {
      auto scalarInfo = ScalarInfo{};

      scalarInfo.size = static_cast<uint32_t>(sizeof(float));
      scalarInfo.offset = static_cast<uint32_t>(
          bufferLayout->getElementVarLayout()->getOffset());
      scalarInfo.type = FromScalarType(bufferLayout->getScalarType());
      break;
    }
    case slang::TypeReflection::Kind::Vector: {
      auto vectorInfo = VectorInfo{};

      vectorInfo.size = static_cast<uint32_t>(sizeof(float) *
                                              bufferLayout->getElementCount());
      vectorInfo.offset = static_cast<uint32_t>(
          bufferLayout->getElementVarLayout()->getOffset());
      vectorInfo.scalarType = FromScalarType(bufferLayout->getScalarType());
      vectorInfo.vectorType = ToVectorType(bufferLayout->getElementCount());

      bufferInfo.info = vectorInfo;
      break;
    }
    case slang::TypeReflection::Kind::Matrix: {
      auto matrixInfo = MatrixInfo{};

      matrixInfo.size =
          static_cast<uint32_t>(sizeof(float) * bufferLayout->getRowCount() *
                                bufferLayout->getColumnCount());
      matrixInfo.offset = static_cast<uint32_t>(
          bufferLayout->getElementVarLayout()->getOffset());
      matrixInfo.matrixType = ToMatrixType(bufferLayout->getRowCount(),
                                           bufferLayout->getColumnCount());

      bufferInfo.info = matrixInfo;
      break;
    }
    default: {
      return Error::Create(
          "Unsupported buffer element type in SSBO reflection.");
    }
    };

    auto resourceInfo = ResourceInfo{};
    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = bufferInfo;

    reflection.resources.emplace_back(resourceInfo);
    reflection
        .slotToInfo[SetBindingToSlot(bufferInfo.set, bufferInfo.binding)] =
        resourceInfo;
  } else {
    return Error::Create("Unsupported resource shape in reflection.");
  }

  return Error::Success();
}

auto SetupFromType(slang::VariableLayoutReflection *variableLayout,
                   ShaderReflection &reflection) -> Error {
  auto *typeLayout = variableLayout->getTypeLayout();
  auto kind = typeLayout->getKind();

  switch (typeLayout->getKind()) {
  case slang::TypeReflection::Kind::Struct: {
    auto paramCount = typeLayout->getFieldCount();
    for (int i = 0; i < paramCount; i++) {
      auto *param = typeLayout->getFieldByIndex(i);
      auto kind = param->getTypeLayout()->getKind();
      auto *typeLayout = param->getTypeLayout();

      auto err = SetupFromType(variableLayout, reflection);

      if (Error::IsError(err)) {
        return err;
      }
    }
    break;
  }
  case slang::TypeReflection::Kind::ConstantBuffer: {
    auto category = variableLayout->getCategory();
    auto *bufferLayout = typeLayout->getElementVarLayout()->getTypeLayout();

    auto access = typeLayout->getResourceAccess();

    bool isPushConstant =
        (category == slang::ParameterCategory::PushConstantBuffer);

    auto bufferInfo = BufferInfo{};

    bufferInfo.name = variableLayout->getName();
    bufferInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    bufferInfo.set = variableLayout->getBindingSpace();
    bufferInfo.binding = variableLayout->getBindingIndex();
    bufferInfo.access = access;
    bufferInfo.bufferType =
        isPushConstant ? BufferType::PushConstant : BufferType::Uniform;

    auto result = SetupStruct(bufferLayout, typeLayout);
    if (Error::IsError(result)) {
      return result.error();
    }

    bufferInfo.info = result.value();

    auto resourceInfo = ResourceInfo{};
    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = bufferInfo;

    reflection.resources.emplace_back(resourceInfo);
    reflection
        .slotToInfo[SetBindingToSlot(bufferInfo.set, bufferInfo.binding)] =
        resourceInfo;
    break;
  }
  case slang::TypeReflection::Kind::Resource: {
    auto err = SetupResource(variableLayout, reflection);
    if (Error::IsError(err)) {
      return err;
    }
    break;
  }
  case slang::TypeReflection::Kind::Array:
    break; // Not right now
  case slang::TypeReflection::Kind::Matrix: {
    auto matrixInfo = MatrixInfo{};
    matrixInfo.size = static_cast<uint32_t>(typeLayout->getSize());
    matrixInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    matrixInfo.matrixType =
        ToMatrixType(typeLayout->getRowCount(), typeLayout->getColumnCount());

    auto resourceInfo = ResourceInfo{};
    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = matrixInfo;

    reflection.resources.emplace_back(resourceInfo);

    break;
  }
  case slang::TypeReflection::Kind::Vector: {
    auto vectorInfo = VectorInfo{};

    vectorInfo.size = static_cast<uint32_t>(typeLayout->getSize());
    vectorInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    vectorInfo.scalarType = FromScalarType(typeLayout->getScalarType());
    vectorInfo.vectorType = ToVectorType(typeLayout->getElementCount());

    auto resourceInfo = ResourceInfo{};
    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = vectorInfo;

    reflection.resources.emplace_back(resourceInfo);

    break;
  }
  case slang::TypeReflection::Kind::Scalar: {
    auto scalarInfo = ScalarInfo{};

    scalarInfo.size = static_cast<uint32_t>(typeLayout->getSize());
    scalarInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    scalarInfo.type = FromScalarType(typeLayout->getScalarType());
    auto resourceInfo = ResourceInfo{};
    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = scalarInfo;

    reflection.resources.emplace_back(resourceInfo);

    break;
  }
  default: {
    auto kindInt = static_cast<int>(typeLayout->getKind());
    return Error::Createf(
        "Unsupported type layout kind for global UBO reflection: {}", kindInt);
  }
  }

  return Error::Success();
}

auto ReflectGlobals(Graphics::GraphicsContext &context,
                    slang::ProgramLayout *programLayout,
                    ShaderReflection &reflection) -> Error {
  VkDescriptorSetLayoutBinding binding = {};

  auto *scopeTypeLayout =
      programLayout->getGlobalParamsVarLayout()->getTypeLayout();

  binding.binding =
      programLayout->getGlobalParamsVarLayout()->getBindingIndex();
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_ALL;

  VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
  layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutCreateInfo.bindingCount = 1;
  layoutCreateInfo.pBindings = &binding;
  VkDescriptorSetLayout descriptorSetLayout = {};
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto result = Error::Create(vkCreateDescriptorSetLayout(
        context.device, &layoutCreateInfo, nullptr, &descriptorSetLayout));
    if (Error::IsError(result)) {
      return result;
    }
  }

  return Error::Success();
}

auto ReflectShader(Graphics::GraphicsContext &context,
                   slang::ProgramLayout *programLayout,
                   ShaderReflection &outReflection) -> Error {

  /// Search parameters ///
  auto parameterCount = programLayout->getParameterCount();

  for (int i = 0; i < parameterCount; ++i) {
    auto *param = programLayout->getParameterByIndex(i);
    auto *typeLayout = param->getTypeLayout();
    const auto *typeName = typeLayout->getType()->getName();
    const auto *paramName = param->getName();

    auto category = param->getCategory();

    auto err = SetupFromType(param, outReflection);
    if (Error::IsError(err)) {
      return err;
    }
  }

  auto *globalParamsLayout = programLayout->getGlobalParamsVarLayout();
  outReflection.ConstructUBOStruct(globalParamsLayout->getBindingSpace(),
                                   globalParamsLayout->getBindingIndex());

  return Error::Success();
}

auto BufferInfo::ToString() const -> std::string {
  std::string result = "Buffer Name: " + name + " Type: ";
  switch (bufferType) {
  case BufferType::Uniform:
    result += "Uniform";
    break;
  case BufferType::Storage:
    result += "Storage";
    break;
  case BufferType::PushConstant:
    result += "Push Constant";
    break;
  default:
    result += "Unknown";
    break;
  }
  result += " Size: " + std::to_string(size) + " Set: " + std::to_string(set) +
            " Binding: " + std::to_string(binding) + "\n";

  if (IsStruct()) {
    const auto &structInfo = std::get<StructInfo>(info);
    result += "  Struct Size: " + std::to_string(structInfo.size) + "\n";
    result += "  Fields:\n";
    for (const auto &field : structInfo.fields) {
      result += "    - " + field.ToString();
    }
  }

  return result;
}