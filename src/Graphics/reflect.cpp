#include "reflect.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"
#include <iostream>

auto ReflectShader(slang::ProgramLayout *programLayout,
                   ShaderReflection &outReflection) -> Error::Error {
  auto parameterCount = programLayout->getParameterCount();

  uint32_t pushConstantSize = 0;
  uint32_t pushConstantOffset = 0;

  for (int i = 0; i < parameterCount; ++i) {
    auto *param = programLayout->getParameterByIndex(i);
    auto *typeLayout = param->getTypeLayout();
    const auto *typeName = typeLayout->getType()->getName();
    const auto *paramName = param->getName();

    std::cout << "Parameter: " << paramName << " Type: " << typeName << "\n";

    auto category = param->getCategory();

    switch (category) {
    case slang::None:
    case slang::Mixed:
    case slang::ConstantBuffer:
    case slang::ShaderResource:
    case slang::UnorderedAccess:
    case slang::VaryingInput:
    case slang::VaryingOutput:
    case slang::SamplerState:
      break;
    case slang::Uniform: {
      ShaderResource resource{};
      resource.name = paramName;
      resource.set = param->getBindingSpace();
      resource.binding = param->getBindingIndex();
      resource.count = 1;
      resource.offset = param->getOffset();
      resource.typeLayout = typeLayout;

      std::cout << "  Offset: " << resource.offset
                << " Size: " << typeLayout->getSize()
                << " ElementCount: " << typeLayout->getElementCount() << "\n";

      outReflection.resources.emplace_back(resource);
      break;
    }
    case slang::DescriptorTableSlot:

    case slang::SpecializationConstant:

    case slang::PushConstantBuffer:

    case slang::RegisterSpace:
    case slang::GenericResource:
    case slang::RayPayload:
    case slang::HitAttributes:
    case slang::CallablePayload:
    case slang::ShaderRecord:
    case slang::ExistentialTypeParam:
    case slang::ExistentialObjectParam:
    case slang::SubElementRegisterSpace:
    case slang::InputAttachmentIndex:

    case slang::MetalArgumentBufferElement:
    case slang::MetalAttribute:
    case slang::MetalPayload:
      break;
    }

    std::cout << "  Category: " << static_cast<int>(category) << "\n";

    switch (typeLayout->getKind()) {
    case slang::TypeReflection::Kind::Scalar:
    case slang::TypeReflection::Kind::Vector:
    case slang::TypeReflection::Kind::Matrix:
    case slang::TypeReflection::Kind::Struct:
    case slang::TypeReflection::Kind::Array:
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
    case slang::TypeReflection::Kind::None:
      break;
    }
  }

  return Error::Success();
}