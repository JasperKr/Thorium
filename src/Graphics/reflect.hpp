#pragma once

#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "hash.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <variant>
#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

enum class ScalarType : uint8_t {
  Unknown,
  Float,
  Int,
  UInt,
  Bool,
};

inline auto FromScalarType(slang::TypeReflection::ScalarType baseType)
    -> ScalarType {
  switch (baseType) {
  case slang::TypeReflection::ScalarType::Float32:
    return ScalarType::Float;
  case slang::TypeReflection::ScalarType::Int32:
    return ScalarType::Int;
  case slang::TypeReflection::ScalarType::UInt32:
    return ScalarType::UInt;
  case slang::TypeReflection::ScalarType::Bool:
    return ScalarType::Bool;
  default:
    return ScalarType::Unknown;
  }
}

// Should be combined with ScalarType to form a complete type
enum class VectorType : uint8_t {
  Unknown,
  Vector2,
  Vector3,
  Vector4,
};

inline auto ToVectorType(uint8_t elementCount) -> VectorType {
  if (elementCount == 2) {
    return VectorType::Vector2;
  }
  if (elementCount == 3) {
    return VectorType::Vector3;
  }
  if (elementCount == 4) {
    return VectorType::Vector4;
  }

  return VectorType::Unknown;
}

enum class MatrixType : uint8_t {
  Unknown,
  Matrix2x2,
  Matrix3x3,
  Matrix4x4,
  Matrix2x3,
  Matrix2x4,
  Matrix3x2,
  Matrix3x4,
  Matrix4x2,
  Matrix4x3,
};

inline auto ToMatrixType(uint8_t rowCount, uint8_t columnCount) -> MatrixType {
  if (rowCount == 2 && columnCount == 2) {
    return MatrixType::Matrix2x2;
  }
  if (rowCount == 3 && columnCount == 3) {
    return MatrixType::Matrix3x3;
  }
  if (rowCount == 4 && columnCount == 4) {
    return MatrixType::Matrix4x4;
  }
  if (rowCount == 2 && columnCount == 3) {
    return MatrixType::Matrix2x3;
  }
  if (rowCount == 2 && columnCount == 4) {
    return MatrixType::Matrix2x4;
  }
  if (rowCount == 3 && columnCount == 2) {
    return MatrixType::Matrix3x2;
  }
  if (rowCount == 3 && columnCount == 4) {
    return MatrixType::Matrix3x4;
  }
  if (rowCount == 4 && columnCount == 2) {
    return MatrixType::Matrix4x2;
  }
  if (rowCount == 4 && columnCount == 3) {
    return MatrixType::Matrix4x3;
  }

  return MatrixType::Unknown;
}

struct ShaderResource {
  std::string name;

  uint32_t set;
  uint32_t binding;

  // For uniform buffers and storage buffers
  uint32_t offset;
  slang::TypeLayoutReflection *typeLayout;

  VkShaderStageFlagBits stage;
  VkDescriptorType type;

  auto operator==(const ShaderResource &other) const -> bool {
    return name == other.name && set == other.set && binding == other.binding &&
           stage == other.stage && type == other.type;
  }
};

struct ShaderResourceHash {
  auto operator()(const ShaderResource &resource) const -> size_t {
    Hash::Hasher hasher;
    hasher.add(std::hash<std::string>()(resource.name));
    hasher.add(std::hash<uint32_t>()(resource.set));
    hasher.add(std::hash<uint32_t>()(resource.binding));
    hasher.add(std::hash<uint32_t>()(static_cast<uint32_t>(resource.stage)));
    hasher.add(std::hash<uint32_t>()(static_cast<uint32_t>(resource.type)));
    return hasher.get();
  }
};

struct PushConstantResource {
  uint32_t offset;
  uint32_t size;
  std::string name;

  auto operator==(const PushConstantResource &other) const -> bool {
    return offset == other.offset && size == other.size && name == other.name;
  }
};

struct PushConstantResourceHash {
  auto operator()(const PushConstantResource &resource) const -> size_t {
    Hash::Hasher hasher;
    hasher.add(std::hash<uint32_t>()(resource.offset));
    hasher.add(std::hash<uint32_t>()(resource.size));
    hasher.add(std::hash<std::string>()(resource.name));
    return hasher.get();
  }
};

struct SamplerInfo {
  uint32_t set;
  uint32_t binding;

  SlangResourceShape shape;
  SlangResourceAccess access;

  [[nodiscard]] auto ToString() const -> std::string {
    std::string str = "SamplerInfo { ";
    str += "set: " + std::to_string(set) + ", ";
    str += "binding: " + std::to_string(binding) + ", ";
    str += "shape: " + std::to_string(static_cast<uint32_t>(shape)) + ", ";
    str += "access: " + std::to_string(static_cast<uint32_t>(access)) + " ";
    str += "}";
    return str;
  }
};

struct ScalarInfo {
  uint32_t size;
  uint32_t offset;

  ScalarType type;

  [[nodiscard]] auto ToString() const -> std::string {
    std::string str = "ScalarInfo { ";
    str += "size: " + std::to_string(size) + ", ";
    str += "offset: " + std::to_string(offset) + ", ";
    str += "type: " + std::to_string(static_cast<uint32_t>(type)) + " ";
    str += "}";
    return str;
  }
};

struct VectorInfo {
  uint32_t size;
  uint32_t offset;

  ScalarType scalarType;
  VectorType vectorType;

  [[nodiscard]] auto ToString() const -> std::string {
    std::string str = "VectorInfo { ";
    str += "size: " + std::to_string(size) + ", ";
    str += "offset: " + std::to_string(offset) + ", ";
    str += "scalarType: " + std::to_string(static_cast<uint32_t>(scalarType)) +
           ", ";
    str += "vectorType: " + std::to_string(static_cast<uint32_t>(vectorType)) +
           " ";
    str += "}";
    return str;
  }
};

struct MatrixInfo {
  uint32_t size;
  uint32_t offset;

  MatrixType matrixType;

  [[nodiscard]] auto ToString() const -> std::string {
    std::string str = "MatrixInfo { ";
    str += "size: " + std::to_string(size) + ", ";
    str += "offset: " + std::to_string(offset) + ", ";
    str += "matrixType: " + std::to_string(static_cast<uint32_t>(matrixType)) +
           " ";
    str += "}";
    return str;
  }
};

enum class StructFieldVariant : uint8_t {
  Unknown,
  Scalar,
  Vector,
  Matrix,
};

struct StructFieldInfo {
  std::string name;

  StructFieldVariant variant;
  std::variant<ScalarInfo, VectorInfo, MatrixInfo> info;
};

struct StructInfo {
  std::vector<StructFieldInfo> fields;
  std::unordered_map<std::string, StructFieldInfo> fieldMap;

  auto ConstructFieldMap() -> void {
    for (const auto &field : fields) {
      fieldMap[field.name] = field;
    }
  }

  [[nodiscard]] auto ToString() const -> std::string {
    std::string str = "StructInfo { ";
    str += "fields: [ ";
    for (const auto &field : fields) {
      str += "{ name: " + field.name + "; type: ";
      switch (field.variant) {
      case StructFieldVariant::Scalar: {
        str += std::get<ScalarInfo>(field.info).ToString();
        break;
      }
      case StructFieldVariant::Vector: {
        str += std::get<VectorInfo>(field.info).ToString();
        break;
      }
      case StructFieldVariant::Matrix: {
        str += std::get<MatrixInfo>(field.info).ToString();
        break;
      }
      case StructFieldVariant::Unknown:
        str += "Unknown";
        break;
      }
    }
    str += "] }";
    return str;
  }
};

enum class StructResourceType : uint8_t {
  Unknown,
  Scalar,
  Vector,
  Matrix,
  Struct,
};

struct BufferInfo {
  uint32_t size;

  uint32_t set;
  uint32_t binding;

  StructResourceType type;
  std::variant<StructInfo, ScalarInfo, VectorInfo, MatrixInfo> info;

  [[nodiscard]] auto ToString() const -> std::string {
    std::string str = "BufferInfo { ";
    str += "size: " + std::to_string(size) + ", ";
    str += "set: " + std::to_string(set) + ", ";
    str += "binding: " + std::to_string(binding) + ", ";
    switch (type) {
    case StructResourceType::Scalar:
      str += "type: " + std::get<ScalarInfo>(info).ToString() + " ";
      break;
    case StructResourceType::Vector:
      str += "type: " + std::get<VectorInfo>(info).ToString() + " ";
      break;
    case StructResourceType::Matrix:
      str += "type: " + std::get<MatrixInfo>(info).ToString() + " ";
      break;
    case StructResourceType::Struct:
      str += "type: " + std::get<StructInfo>(info).ToString() + " ";
      break;
    default:
      str += "type: Unknown ";
      break;
    }
    str += "}";
    return str;
  }
};

// Resource containers //

enum class ResourceVariant : uint8_t {
  Unknown,
  Sampler,
  Scalar,
  Vector,
  Matrix,
  Buffer,
};

struct ResourceInfo {
  std::string name;
  // SlangStage stage;

  ResourceVariant variant;
  std::variant<SamplerInfo, ScalarInfo, VectorInfo, MatrixInfo, BufferInfo>
      info;
};

struct ShaderReflection {
  std::vector<PushConstantResource> pushConstants;
  std::vector<ResourceInfo> resources;
  std::unordered_map<std::string, ResourceInfo> resourceMap;

  // NOLINTNEXTLINE
  auto ConstructUBOStruct(uint32_t set, uint32_t binding) -> void {
    StructInfo globalUBOStruct = {};

    if (resources.size() == 0) {
      return;
    }

    uint32_t size = 0;

    for (int i = static_cast<int>(resources.size()) - 1; i >= 0; i--) {
      const auto &resource = resources[i];
      if (resource.variant == ResourceVariant::Scalar) {
        auto info = std::get<ScalarInfo>(resource.info);
        globalUBOStruct.fields.emplace_back(StructFieldInfo{
            .name = resource.name,
            .variant = StructFieldVariant::Scalar,
            .info = info,
        });
        resources.erase(resources.begin() + static_cast<uint32_t>(i));
        size = (std::max)(size, info.offset + info.size);
      } else if (resource.variant == ResourceVariant::Vector) {
        auto info = std::get<VectorInfo>(resource.info);
        globalUBOStruct.fields.emplace_back(StructFieldInfo{
            .name = resource.name,
            .variant = StructFieldVariant::Vector,
            .info = info,
        });
        resources.erase(resources.begin() + static_cast<uint32_t>(i));
        size = (std::max)(size, info.offset + info.size);
      } else if (resource.variant == ResourceVariant::Matrix) {
        auto info = std::get<MatrixInfo>(resource.info);
        globalUBOStruct.fields.emplace_back(StructFieldInfo{
            .name = resource.name,
            .variant = StructFieldVariant::Matrix,
            .info = info,
        });
        resources.erase(resources.begin() + static_cast<uint32_t>(i));
        size = (std::max)(size, info.offset + info.size);
      }
    }

    if (globalUBOStruct.fields.size() == 0) {
      return;
    }

    std::cout << "Constructed global UBO struct:\n";

    globalUBOStruct.ConstructFieldMap();

    BufferInfo globalUBOInfo{
        .size = size,
        .set = set,
        .binding = binding,
        .type = StructResourceType::Struct,
        .info = globalUBOStruct,
    };

    ResourceInfo globalUBOResource{
        .name = "GlobalUBO",
        .variant = ResourceVariant::Buffer,
        .info = globalUBOInfo,
    };

    resources.emplace_back(globalUBOResource);
  }
};

auto ReflectShader(Graphics::GraphicsContext &context,
                   slang::ProgramLayout *programLayout,
                   ShaderReflection &outReflection) -> Error::Error;