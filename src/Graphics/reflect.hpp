#pragma once

#include "Graphics/graphics.hpp"
#include "Modules/console.hpp"
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

inline static auto AlignUp(size_t offset, size_t align) -> size_t {
  return (offset + align - 1) & ~(align - 1);
}

inline static auto BaseAlignment(uint8_t componentCount) -> size_t {
  switch (componentCount) {
  case 2:
    return 8; // NOLINT
  case 3:
  case 4:
    return 16; // NOLINT
  default:
    return 4;
  }
}

inline static auto SizeOf(VectorType vectorType) -> size_t {
  switch (vectorType) {
  case VectorType::Vector2:
    return sizeof(float) * 2;
  case VectorType::Vector3:
    return sizeof(float) * 3;
  case VectorType::Vector4:
    return sizeof(float) * 4;
  default:
    return sizeof(float);
  }
}

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

  void ToString(IndentedPrinter &printer) const {
    printer *= "SamplerInfo {";
    printer += "set: " + std::to_string(set) + ", ";
    printer *= "binding: " + std::to_string(binding) + ", ";
    printer *= "shape: " + std::to_string(static_cast<uint32_t>(shape)) + ", ";
    printer *= "access: " + std::to_string(static_cast<uint32_t>(access)) + "";
    printer -= "}";
  }
};

struct ScalarInfo {
  uint32_t size;
  uint32_t offset;

  ScalarType type;

  void ToString(IndentedPrinter &printer) const {
    printer *= "ScalarInfo {";
    printer += "size: " + std::to_string(size) + ", ";
    printer *= "offset: " + std::to_string(offset) + ", ";
    printer *= "type: " + std::to_string(static_cast<uint32_t>(type)) + "";
    printer -= "}";
  }
};

struct VectorInfo {
  uint32_t size;
  uint32_t offset;

  ScalarType scalarType;
  VectorType vectorType;

  void ToString(IndentedPrinter &printer) const {
    printer *= "VectorInfo {";
    printer += "size: " + std::to_string(size) + ", ";
    printer *= "offset: " + std::to_string(offset) + ", ";
    printer *=
        "scalarType: " + std::to_string(static_cast<uint32_t>(scalarType)) +
        ", ";
    printer *=
        "vectorType: " + std::to_string(static_cast<uint32_t>(vectorType)) + "";
    printer -= "}";
  }
};

struct MatrixInfo {
  uint32_t size;
  uint32_t offset;

  MatrixType matrixType;

  void ToString(IndentedPrinter &printer) const {
    printer *= "MatrixInfo {";
    printer += "size: " + std::to_string(size) + ", ";
    printer *= "offset: " + std::to_string(offset) + ", ";
    printer *=
        "matrixType: " + std::to_string(static_cast<uint32_t>(matrixType)) + "";
    printer -= "}";
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

  [[nodiscard]] auto GetSize() const -> size_t {
    switch (variant) {
    case StructFieldVariant::Scalar:
      return std::get<ScalarInfo>(info).size;
    case StructFieldVariant::Vector:
      return std::get<VectorInfo>(info).size;
    case StructFieldVariant::Matrix:
      return std::get<MatrixInfo>(info).size;
    default:
      return 0;
    }
  }

  [[nodiscard]] auto GetOffset() const -> size_t {
    switch (variant) {
    case StructFieldVariant::Scalar:
      return std::get<ScalarInfo>(info).offset;
    case StructFieldVariant::Vector:
      return std::get<VectorInfo>(info).offset;
    case StructFieldVariant::Matrix:
      return std::get<MatrixInfo>(info).offset;
    default:
      return 0;
    }
  }
};

struct StructInfo {
  std::vector<StructFieldInfo> fields;
  std::unordered_map<std::string, StructFieldInfo> fieldMap;
  uint32_t size;
  uint32_t alignment;

  auto ConstructFieldMap() -> void {
    for (const auto &field : fields) {
      fieldMap[field.name] = field;
    }
  }

  void ToString(IndentedPrinter &printer) const {
    printer *= "StructInfo";
    printer *= "Size: " + std::to_string(size) + ",";
    printer *= "Alignment: " + std::to_string(alignment) + ",";
    printer *= "Fields: ";
    printer *= "{";
    printer.Indent();
    for (const auto &field : fields) {
      printer *= "{";
      printer += "name: " + field.name;
      printer *= "type: ";
      printer.Inline();
      switch (field.variant) {
      case StructFieldVariant::Scalar: {
        std::get<ScalarInfo>(field.info).ToString(printer);
        break;
      }
      case StructFieldVariant::Vector: {
        std::get<VectorInfo>(field.info).ToString(printer);
        break;
      }
      case StructFieldVariant::Matrix: {
        std::get<MatrixInfo>(field.info).ToString(printer);
        break;
      }
      case StructFieldVariant::Unknown:
        printer *= "Unknown";
        break;
      }

      printer -= "},";
    }
    printer -= "},";
  }
};

enum class BufferResourceType : uint8_t {
  Unknown,
  Scalar,
  Vector,
  Matrix,
  Struct,
};

enum class BufferType : uint8_t {
  Unknown,
  Uniform,
  Storage,
  PushConstant,
};

struct BufferInfo {
  std::string name;

  uint32_t size;
  uint32_t offset; // For push constants

  uint32_t set;
  uint32_t binding;

  BufferResourceType type;
  SlangResourceAccess access;
  BufferType bufferType;
  std::variant<StructInfo, ScalarInfo, VectorInfo, MatrixInfo> info;

  void ToString(IndentedPrinter &printer) const {
    printer *= "BufferInfo";
    printer *= "{";
    printer += "size: " + std::to_string(size) + ", ";
    printer *= "set: " + std::to_string(set) + ", ";
    printer *= "binding: " + std::to_string(binding) + ", ";
    printer *=
        "access: " + std::to_string(static_cast<uint32_t>(access)) + ", ";
    printer *=
        "bufferType: " + std::to_string(static_cast<uint32_t>(bufferType)) +
        ", ";
    printer *= "type: ";
    printer.Inline();
    switch (type) {
    case BufferResourceType::Scalar:
      std::get<ScalarInfo>(info).ToString(printer);
      break;
    case BufferResourceType::Vector:
      std::get<VectorInfo>(info).ToString(printer);
      break;
    case BufferResourceType::Matrix:
      std::get<MatrixInfo>(info).ToString(printer);
      break;
    case BufferResourceType::Struct:
      std::get<StructInfo>(info).ToString(printer);
      break;
    default:
      printer *= "Unknown";
      break;
    }
    printer -= "},";
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

  ResourceVariant variant;
  std::variant<SamplerInfo, ScalarInfo, VectorInfo, MatrixInfo, BufferInfo>
      info;
};

struct ShaderReflection {
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

    globalUBOStruct.ConstructFieldMap();

    BufferInfo globalUBOInfo{
        .size = size,
        .set = set,
        .binding = binding,
        .type = BufferResourceType::Struct,
        .access = SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ,
        .bufferType = BufferType::Uniform,
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