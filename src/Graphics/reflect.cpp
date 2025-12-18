#include "reflect.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"
#include "vulkan/vulkan_core.h"

struct FindShaderParameterResult {
  bool found = false;
  slang::TypeLayoutReflection *typeLayout = nullptr;
};

auto FindShaderParameter(slang::ProgramLayout *programLayout,
                         const std::string &parameterName)
    -> FindShaderParameterResult {
  auto parameterCount = programLayout->getParameterCount();

  /// Search parameters ///
  for (int i = 0; i < parameterCount; ++i) {
    auto *param = programLayout->getParameterByIndex(i);
    const auto *paramName = param->getName();

    if (paramName != nullptr && parameterName == paramName) {
      FindShaderParameterResult result{};
      result.found = true;
      result.typeLayout = param->getTypeLayout();
      return result;
    }
  }

  // Search Global variables //
  auto *globalParamsLayout = programLayout->getGlobalParamsVarLayout();
  auto *globalParamsTypeLayout = globalParamsLayout->getTypeLayout();

  // If there is only 1 item, it's not a struct
  const auto *name = globalParamsTypeLayout->getName();
  if (name != nullptr && parameterName == name) {
    FindShaderParameterResult result{};
    result.found = true;
    result.typeLayout = globalParamsTypeLayout;
    return result;
  }

  // Otherwise, search fields
  auto fieldCount = globalParamsTypeLayout->getFieldCount();
  for (int i = 0; i < fieldCount; ++i) {
    auto *field = globalParamsTypeLayout->getFieldByIndex(i);
    const auto *fieldName = field->getName();

    if (fieldName != nullptr && parameterName == fieldName) {
      FindShaderParameterResult result{};
      result.found = true;
      result.typeLayout = field->getTypeLayout();
      return result;
    }
  }

  return FindShaderParameterResult{};
}

auto SetupStruct(slang::TypeLayoutReflection *bufferLayout,
                 slang::TypeLayoutReflection *typeLayout, BufferInfo &info)
    -> Error::Error {
  switch (bufferLayout->getKind()) {
  case slang::TypeReflection::Kind::Struct: {
    info.type = BufferResourceType::Struct;
    StructInfo structInfo{
        .size = static_cast<uint32_t>(bufferLayout->getSize()),
        .alignment = static_cast<uint32_t>(bufferLayout->getAlignment()),
    };
    auto fieldCount = bufferLayout->getFieldCount();

    for (int i = 0; i < fieldCount; ++i) {
      auto *fieldVariableType = bufferLayout->getFieldByIndex(i);
      auto *fieldType = fieldVariableType->getTypeLayout();

      switch (fieldType->getKind()) {
      case slang::TypeReflection::Kind::Scalar: {
        ScalarInfo scalarInfo{
            .size = static_cast<uint32_t>(fieldType->getSize()),
            .offset = static_cast<uint32_t>(fieldVariableType->getOffset()),
            .type = FromScalarType(fieldType->getScalarType()),
        };

        StructFieldInfo fieldInfo{
            .name = fieldVariableType->getName(),
            .variant = StructFieldVariant::Scalar,
            .info = scalarInfo,
        };

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Vector: {
        VectorInfo vectorInfo{
            .size = static_cast<uint32_t>(fieldType->getSize()),
            .offset = static_cast<uint32_t>(fieldVariableType->getOffset()),
            .scalarType = FromScalarType(fieldType->getScalarType()),
            .vectorType = ToVectorType(fieldType->getElementCount()),
        };

        StructFieldInfo fieldInfo{
            .name = fieldVariableType->getName(),
            .variant = StructFieldVariant::Vector,
            .info = vectorInfo,
        };

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Matrix: {
        MatrixInfo matrixInfo{
            .size = static_cast<uint32_t>(fieldType->getSize()),
            .offset = static_cast<uint32_t>(fieldVariableType->getOffset()),
            .matrixType = ToMatrixType(fieldType->getRowCount(),
                                       fieldType->getColumnCount()),
        };

        StructFieldInfo fieldInfo{
            .name = fieldVariableType->getName(),
            .variant = StructFieldVariant::Matrix,
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

    structInfo.ConstructFieldMap();
    info.info = structInfo;

    break;
  }
  case slang::TypeReflection::Kind::Scalar: {
    info.type = BufferResourceType::Scalar;

    ScalarInfo scalarInfo{
        .size = static_cast<uint32_t>(bufferLayout->getSize()),
        .offset = static_cast<uint32_t>(
            typeLayout->getElementVarLayout()->getOffset()),
        .type = FromScalarType(bufferLayout->getScalarType()),
    };

    info.info = scalarInfo;

    break;
  }
  case slang::TypeReflection::Kind::Vector: {
    info.type = BufferResourceType::Vector;

    VectorInfo vectorInfo{
        .size = static_cast<uint32_t>(bufferLayout->getSize()),
        .offset = static_cast<uint32_t>(
            typeLayout->getElementVarLayout()->getOffset()),
        .scalarType = FromScalarType(bufferLayout->getScalarType()),
        .vectorType = ToVectorType(bufferLayout->getElementCount()),
    };

    info.info = vectorInfo;
    break;
  }
  case slang::TypeReflection::Kind::Matrix: {
    info.type = BufferResourceType::Matrix;

    MatrixInfo matrixInfo{
        .size = static_cast<uint32_t>(bufferLayout->getSize()),
        .offset = static_cast<uint32_t>(
            typeLayout->getElementVarLayout()->getOffset()),
        .matrixType = ToMatrixType(bufferLayout->getRowCount(),
                                   bufferLayout->getColumnCount()),
    };

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

inline auto SlangStageToVkStage(SlangStage stage) {
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
                   ShaderReflection &reflection) -> Error::Error {

  auto *typeLayout = variableLayout->getTypeLayout();
  auto kind = typeLayout->getKind();
  auto shape = typeLayout->getResourceShape();
  auto access = typeLayout->getResourceAccess();

  // Masked out shape flags
  auto maskedShape = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;

  if (maskedShape == SLANG_TEXTURE_1D || maskedShape == SLANG_TEXTURE_2D ||
      maskedShape == SLANG_TEXTURE_3D || maskedShape == SLANG_TEXTURE_CUBE ||
      maskedShape == SLANG_TEXTURE_BUFFER) {

    SamplerInfo samplerInfo{
        .set = variableLayout->getBindingSpace(),
        .binding = variableLayout->getBindingIndex(),
        .shape = shape,
        .access = access,
    };

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        .stages = SlangStageToVkStage(variableLayout->getStage()),
        .variant = ResourceVariant::Sampler,
        .info = samplerInfo,
    };

    reflection.resources.emplace_back(resourceInfo);
  } else if (maskedShape == SLANG_STRUCTURED_BUFFER ||
             maskedShape == SLANG_BYTE_ADDRESS_BUFFER) {
    // SSBO
    BufferInfo info{
        .name = variableLayout->getName(),
        .set = variableLayout->getBindingSpace(),
        .binding = variableLayout->getBindingIndex(),
        .access = access,
        .bufferType = BufferType::Storage,
    };

    auto *bufferLayout =
        variableLayout->getTypeLayout()->getElementTypeLayout();

    switch (bufferLayout->getKind()) {
    case slang::TypeReflection::Kind::Struct: {
      info.type = BufferResourceType::Struct;
      StructInfo structInfo{
          .size = static_cast<uint32_t>(bufferLayout->getSize()),
          .alignment = static_cast<uint32_t>(bufferLayout->getAlignment()),
      };
      auto fieldCount = bufferLayout->getFieldCount();

      for (int i = 0; i < fieldCount; ++i) {
        auto *fieldVariableType = bufferLayout->getFieldByIndex(i);
        auto *fieldType = fieldVariableType->getType();

        switch (fieldType->getKind()) {
        case slang::TypeReflection::Kind::Scalar: {
          ScalarInfo scalarInfo{
              .size = sizeof(float),
              .offset = static_cast<uint32_t>(fieldVariableType->getOffset()),
              .type = FromScalarType(fieldType->getScalarType()),
          };

          StructFieldInfo fieldInfo{
              .name = fieldVariableType->getName(),
              .variant = StructFieldVariant::Scalar,
              .info = scalarInfo,
          };

          structInfo.fields.emplace_back(fieldInfo);

          break;
        }
        case slang::TypeReflection::Kind::Vector: {
          VectorInfo vectorInfo{
              .size = static_cast<uint32_t>(sizeof(float) *
                                            fieldType->getElementCount()),
              .offset = static_cast<uint32_t>(fieldVariableType->getOffset()),
              .scalarType = FromScalarType(fieldType->getScalarType()),
              .vectorType = ToVectorType(fieldType->getElementCount()),
          };

          StructFieldInfo fieldInfo{
              .name = fieldVariableType->getName(),
              .variant = StructFieldVariant::Vector,
              .info = vectorInfo,
          };

          structInfo.fields.emplace_back(fieldInfo);

          break;
        }
        case slang::TypeReflection::Kind::Matrix: {
          MatrixInfo matrixInfo{
              .size = static_cast<uint32_t>(sizeof(float) *
                                            fieldType->getRowCount() *
                                            fieldType->getColumnCount()),
              .offset = static_cast<uint32_t>(fieldVariableType->getOffset()),
              .matrixType = ToMatrixType(fieldType->getRowCount(),
                                         fieldType->getColumnCount()),
          };

          StructFieldInfo fieldInfo{
              .name = fieldVariableType->getName(),
              .variant = StructFieldVariant::Matrix,
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

      structInfo.ConstructFieldMap();
      info.info = structInfo;

      break;
    }
    case slang::TypeReflection::Kind::Scalar: {
      info.type = BufferResourceType::Scalar;
      ScalarInfo scalarInfo{
          .size = static_cast<uint32_t>(sizeof(float)),
          .offset = static_cast<uint32_t>(
              bufferLayout->getElementVarLayout()->getOffset()),
          .type = FromScalarType(bufferLayout->getScalarType()),
      };
      info.info = scalarInfo;
      break;
    }
    case slang::TypeReflection::Kind::Vector: {
      info.type = BufferResourceType::Vector;
      VectorInfo vectorInfo{
          .size = static_cast<uint32_t>(sizeof(float) *
                                        bufferLayout->getElementCount()),
          .offset = static_cast<uint32_t>(
              bufferLayout->getElementVarLayout()->getOffset()),
          .scalarType = FromScalarType(bufferLayout->getScalarType()),
          .vectorType = ToVectorType(bufferLayout->getElementCount()),
      };

      info.info = vectorInfo;
      break;
    }
    case slang::TypeReflection::Kind::Matrix: {
      info.type = BufferResourceType::Matrix;
      MatrixInfo matrixInfo{
          .size = static_cast<uint32_t>(sizeof(float) *
                                        bufferLayout->getRowCount() *
                                        bufferLayout->getColumnCount()),
          .offset = static_cast<uint32_t>(
              bufferLayout->getElementVarLayout()->getOffset()),
          .matrixType = ToMatrixType(bufferLayout->getRowCount(),
                                     bufferLayout->getColumnCount()),
      };

      info.info = matrixInfo;
      break;
    }
    default: {
      return Error::Create(
          "Unsupported buffer element type in SSBO reflection.");
    }
    };

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        .stages = SlangStageToVkStage(variableLayout->getStage()),
        .variant = ResourceVariant::Buffer,
        .info = info,
    };

    reflection.resources.emplace_back(resourceInfo);
  } else {
    return Error::Create("Unsupported resource shape in reflection.");
  }

  return Error::Success();
}

auto SetupFromType(slang::VariableLayoutReflection *variableLayout,
                   ShaderReflection &reflection) -> Error::Error {
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

    BufferInfo info{
        .name = variableLayout->getName(),
        .offset = static_cast<uint32_t>(variableLayout->getOffset()),
        .set = variableLayout->getBindingSpace(),
        .binding = variableLayout->getBindingIndex(),
        .access = access,
        .bufferType =
            isPushConstant ? BufferType::PushConstant : BufferType::Uniform,
    };

    auto err = SetupStruct(bufferLayout, typeLayout, info);
    if (Error::IsError(err)) {
      return err;
    }

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        .stages = SlangStageToVkStage(variableLayout->getStage()),
        .variant = ResourceVariant::Buffer,
        .info = info,
    };

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
    MatrixInfo matrixInfo{
        .size = static_cast<uint32_t>(typeLayout->getSize()),
        .offset = static_cast<uint32_t>(variableLayout->getOffset()),
        .matrixType = ToMatrixType(typeLayout->getRowCount(),
                                   typeLayout->getColumnCount()),
    };

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        .stages = SlangStageToVkStage(variableLayout->getStage()),
        .variant = ResourceVariant::Matrix,
        .info = matrixInfo,
    };

    reflection.resources.emplace_back(resourceInfo);

    break;
  }
  case slang::TypeReflection::Kind::Vector: {
    VectorInfo vectorInfo{
        .size = static_cast<uint32_t>(typeLayout->getSize()),
        .offset = static_cast<uint32_t>(variableLayout->getOffset()),
        .scalarType = FromScalarType(typeLayout->getScalarType()),
        .vectorType = ToVectorType(typeLayout->getElementCount()),
    };

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        .stages = SlangStageToVkStage(variableLayout->getStage()),
        .variant = ResourceVariant::Vector,
        .info = vectorInfo,
    };

    reflection.resources.emplace_back(resourceInfo);

    break;
  }
  case slang::TypeReflection::Kind::Scalar: {
    ScalarInfo scalarInfo{
        .size = static_cast<uint32_t>(typeLayout->getSize()),
        .offset = static_cast<uint32_t>(variableLayout->getOffset()),
        .type = FromScalarType(typeLayout->getScalarType()),
    };

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        .stages = SlangStageToVkStage(variableLayout->getStage()),
        .variant = ResourceVariant::Scalar,
        .info = scalarInfo,
    };

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
                    ShaderReflection &reflection) -> Error::Error {
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
                   ShaderReflection &outReflection) -> Error::Error {

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