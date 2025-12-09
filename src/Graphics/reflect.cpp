#include "reflect.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"
#include "vulkan/vulkan_core.h"
#include <iostream>

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

auto SetupFromType(slang::VariableLayoutReflection *variableLayout,
                   ShaderReflection &reflection) -> Error::Error {
  auto *typeLayout = variableLayout->getTypeLayout();
  auto kind = typeLayout->getKind();

  std::cout << "Setting up: " << variableLayout->getName() << "\n";

  switch (typeLayout->getKind()) {
  case slang::TypeReflection::Kind::Struct: {
    std::cout << "parameters: " << "\n";

    auto paramCount = typeLayout->getFieldCount();
    for (int i = 0; i < paramCount; i++) {
      std::cout << "- ";

      auto *param = typeLayout->getFieldByIndex(i);
      std::cout << param->getName() << "\n";

      auto kind = param->getTypeLayout()->getKind();
      std::cout << "  Kind: " << static_cast<int>(kind) << "\n";
      auto *typeLayout = param->getTypeLayout();

      auto err = SetupFromType(variableLayout, reflection);

      if (Error::IsError(err)) {
        return err;
      }
    }
    break;
  }
  case slang::TypeReflection::Kind::ConstantBuffer: {
    auto *bufferLayout = typeLayout->getElementVarLayout()->getTypeLayout();

    BufferInfo info{
        .set = variableLayout->getBindingSpace(),
        .binding = variableLayout->getBindingIndex(),
    };

    switch (bufferLayout->getKind()) {
    case slang::TypeReflection::Kind::Struct: {
      info.type = StructResourceType::Struct;
      StructInfo structInfo{};
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
      info.type = StructResourceType::Scalar;

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
      info.type = StructResourceType::Vector;

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
      info.type = StructResourceType::Matrix;

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

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        .variant = ResourceVariant::Buffer,
        .info = info,
    };

    reflection.resources.emplace_back(resourceInfo);
    break;
  }
  case slang::TypeReflection::Kind::Resource: {
    auto shape = typeLayout->getResourceShape();
    auto access = typeLayout->getResourceAccess();

    SamplerInfo samplerInfo{
        .set = variableLayout->getBindingSpace(),
        .binding = variableLayout->getBindingIndex(),
        .shape = shape,
        .access = access,
    };

    ResourceInfo resourceInfo = {
        .name = variableLayout->getName(),
        // .stage = SlangStage::SLANG_STAGE_FRAGMENT,
        .variant = ResourceVariant::Sampler,
        .info = samplerInfo,
    };

    reflection.resources.emplace_back(resourceInfo);

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

  // auto result = ReflectGlobals(context, programLayout, outReflection);

  uint32_t pushConstantSize = 0;
  uint32_t pushConstantOffset = 0;

  /// Search parameters ///
  auto parameterCount = programLayout->getParameterCount();

  for (int i = 0; i < parameterCount; ++i) {
    auto *param = programLayout->getParameterByIndex(i);
    auto *typeLayout = param->getTypeLayout();
    const auto *typeName = typeLayout->getType()->getName();
    const auto *paramName = param->getName();

    std::cout << "Parameter: " << (paramName != nullptr ? paramName : "null")
              << ", Type: " << (typeName != nullptr ? typeName : "null")
              << "\n";

    auto category = param->getCategory();

    auto err = SetupFromType(param, outReflection);
    if (Error::IsError(err)) {
      return err;
    }
  }

  auto *globalParamsLayout = programLayout->getGlobalParamsVarLayout();
  outReflection.ConstructUBOStruct(globalParamsLayout->getBindingSpace(),
                                   globalParamsLayout->getBindingIndex());

  for (auto &resource : outReflection.resources) {
    outReflection.resourceMap[resource.name] = resource;
    std::cout << "Reflected resource: " << resource.name << ": ";

    switch (resource.variant) {
    case ResourceVariant::Sampler: {
      auto info = std::get<SamplerInfo>(resource.info);
      std::cout << info.ToString() << "\n";
      break;
    }
    case ResourceVariant::Scalar: {
      auto info = std::get<ScalarInfo>(resource.info);
      std::cout << info.ToString() << "\n";
      break;
    }
    case ResourceVariant::Vector: {
      auto info = std::get<VectorInfo>(resource.info);
      std::cout << info.ToString() << "\n";
      break;
    }
    case ResourceVariant::Matrix: {
      auto info = std::get<MatrixInfo>(resource.info);
      std::cout << info.ToString() << "\n";
      break;
    }
    case ResourceVariant::Buffer: {
      auto info = std::get<BufferInfo>(resource.info);
      std::cout << info.ToString() << "\n";
      break;
    }
    default: {
      std::cout << "Unknown resource variant\n";
      break;
    }
    }
  }

  // if (Error::IsError(result)) {
  //   return result;
  // }

  return Error::Create("Stop");
}