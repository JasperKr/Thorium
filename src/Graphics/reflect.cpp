#include "reflect.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"
#define VK_NO_PROTOTYPES
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
                 slang::TypeLayoutReflection *typeLayout, BufferInfo &info)
    -> Error {
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
      default: {
        return Error::Create(
            "Unsupported struct field type in Buffer struct reflection.");
      }
      }
    }

    info.info = structInfo;

    break;
  }
  case slang::TypeReflection::Kind::Scalar: {
    auto scalarInfo = ScalarInfo{};
    scalarInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
    scalarInfo.offset =
        static_cast<uint32_t>(typeLayout->getElementVarLayout()->getOffset());
    info.info = scalarInfo;

    break;
  }
  case slang::TypeReflection::Kind::Vector: {
    auto vectorInfo = VectorInfo{};

    vectorInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
    vectorInfo.offset =
        static_cast<uint32_t>(typeLayout->getElementVarLayout()->getOffset());
    vectorInfo.scalarType = FromScalarType(bufferLayout->getScalarType());
    vectorInfo.vectorType = ToVectorType(bufferLayout->getElementCount());

    info.info = vectorInfo;
    break;
  }
  case slang::TypeReflection::Kind::Matrix: {
    auto matrixInfo = MatrixInfo{};

    matrixInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
    matrixInfo.offset =
        static_cast<uint32_t>(typeLayout->getElementVarLayout()->getOffset());
    matrixInfo.matrixType = ToMatrixType(bufferLayout->getRowCount(),
                                         bufferLayout->getColumnCount());

    info.info = matrixInfo;
    break;
  }
  default: {
    return Error::Create(
        "Unsupported buffer element type in Buffer reflection.");
  }
  }

  return Error::Success();
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

    auto resourceInfo = ResourceInfo{};

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = samplerInfo;

    reflection.resources.emplace_back(resourceInfo);
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

    auto err = SetupStruct(bufferLayout, typeLayout, bufferInfo);
    if (Error::IsError(err)) {
      return err;
    }

    auto resourceInfo = ResourceInfo{};
    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = bufferInfo;

    reflection.resources.emplace_back(resourceInfo);
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
    auto resourceInfo = ResourceInfo{};
    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = scalarInfo;

    reflection.resources.emplace_back(resourceInfo);

    break;
  }
  default: {
    return Error::Create(
        "Unsupported type layout kind for global UBO reflection.");
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
  auto result = Error::Create(vkCreateDescriptorSetLayout(
      context.device, &layoutCreateInfo, nullptr, &descriptorSetLayout));
  if (Error::IsError(result)) {
    return result;
  }

  return Error::Success();

  // return descriptorSetLayout;
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