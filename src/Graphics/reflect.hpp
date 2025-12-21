#pragma once

#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <string>
#include <sys/types.h>
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

struct ResourceKey {
  std::string name;
  ResourceKey *child;
};

struct SamplerInfo {
  uint32_t set;
  uint32_t binding;

  SlangResourceShape shape;
  SlangResourceAccess access;
};

struct ScalarInfo {
  uint32_t size;
  uint32_t offset;

  ScalarType type;
};

struct VectorInfo {
  uint32_t size;
  uint32_t offset;

  ScalarType scalarType;
  VectorType vectorType;
};

struct MatrixInfo {
  uint32_t size;
  uint32_t offset;

  MatrixType matrixType;
};

enum class StructFieldVariant : uint8_t {
  Unknown,
  Scalar,
  Vector,
  Matrix,
  Struct,
};

struct ResourceInfo;

struct StructInfo {
  std::vector<ResourceInfo> fields;
  std::vector<uint32_t> fieldOffsets;

  uint32_t size;
  uint32_t alignment;

  auto Find(const ResourceKey &key) -> ResourceInfo *;
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

  SlangResourceAccess access;
  BufferType bufferType;

  [[nodiscard]] constexpr auto IsStruct() const -> bool {
    return std::holds_alternative<StructInfo>(info);
  }
  [[nodiscard]] constexpr auto IsScalar() const -> bool {
    return std::holds_alternative<ScalarInfo>(info);
  }
  [[nodiscard]] constexpr auto IsVector() const -> bool {
    return std::holds_alternative<VectorInfo>(info);
  }
  [[nodiscard]] constexpr auto IsMatrix() const -> bool {
    return std::holds_alternative<MatrixInfo>(info);
  }

  template <typename T>
  [[nodiscard]] constexpr auto GetInfo() const -> const T & {
    return std::get<T>(info);
  }
  template <typename T> [[nodiscard]] constexpr auto Is() const -> bool {
    return std::holds_alternative<T>(info);
  }

  std::variant<StructInfo, ScalarInfo, VectorInfo, MatrixInfo> info;
};

struct ResourceInfo {
  std::string name;
  VkShaderStageFlags stages = VK_SHADER_STAGE_ALL;
  uint32_t offset{0};

  [[nodiscard]] constexpr auto IsBuffer() const -> bool {
    return std::holds_alternative<BufferInfo>(info);
  }
  [[nodiscard]] constexpr auto IsSampler() const -> bool {
    return std::holds_alternative<SamplerInfo>(info);
  }
  [[nodiscard]] constexpr auto IsStruct() const -> bool {
    return std::holds_alternative<StructInfo>(info);
  }
  [[nodiscard]] constexpr auto IsScalar() const -> bool {
    return std::holds_alternative<ScalarInfo>(info);
  }
  [[nodiscard]] constexpr auto IsVector() const -> bool {
    return std::holds_alternative<VectorInfo>(info);
  }
  [[nodiscard]] constexpr auto IsMatrix() const -> bool {
    return std::holds_alternative<MatrixInfo>(info);
  }
  template <typename T>
  [[nodiscard]] constexpr auto GetInfo() const -> const T & {
    return std::get<T>(info);
  }
  template <typename T> [[nodiscard]] constexpr auto Is() const -> bool {
    return std::holds_alternative<T>(info);
  }
  [[nodiscard]] auto GetOffset() const -> uint32_t { return offset; }
  [[nodiscard]] constexpr auto GetSize() const -> uint32_t {
    if (IsBuffer()) {
      const auto &bufferInfo = std::get<BufferInfo>(info);
      return bufferInfo.size;
    }
    if (IsStruct()) {
      const auto &structInfo = std::get<StructInfo>(info);
      return structInfo.size;
    }
    if (IsScalar()) {
      const auto &scalarInfo = std::get<ScalarInfo>(info);
      return scalarInfo.size;
    }
    if (IsVector()) {
      const auto &vectorInfo = std::get<VectorInfo>(info);
      return vectorInfo.size;
    }
    if (IsMatrix()) {
      const auto &matrixInfo = std::get<MatrixInfo>(info);
      return matrixInfo.size;
    }

    return 0;
  }

  std::variant<SamplerInfo, ScalarInfo, VectorInfo, MatrixInfo, BufferInfo,
               StructInfo>
      info;
};

struct ShaderReflection {
  std::vector<ResourceInfo> resources;
  BufferInfo globals;
  bool hasGlobals{false};

  // NOLINTNEXTLINE
  auto ConstructUBOStruct(uint32_t set, uint32_t binding) -> void {
    ResourceInfo globalUBOInfo{};
    globalUBOInfo.name = "Globals";
    globalUBOInfo.stages = VK_SHADER_STAGE_ALL;
    globalUBOInfo.info = StructInfo{};
    auto globalUBOStruct = std::get<StructInfo>(globalUBOInfo.info);

    if (resources.size() == 0) {
      return;
    }

    uint32_t size = 0;

    for (int i = static_cast<int>(resources.size()) - 1; i >= 0; i--) {
      const auto &resource = resources[i];
      if (std::holds_alternative<ScalarInfo>(resource.info)) {
        auto info = std::get<ScalarInfo>(resource.info);
        globalUBOStruct.fields.emplace_back(ResourceInfo{
            .name = resource.name,
            .info = info,
        });
        resources.erase(resources.begin() + static_cast<uint32_t>(i));
        size = (std::max)(size, info.offset + info.size);
      } else if (std::holds_alternative<VectorInfo>(resource.info)) {
        auto info = std::get<VectorInfo>(resource.info);
        globalUBOStruct.fields.emplace_back(ResourceInfo{
            .name = resource.name,
            .info = info,
        });
        resources.erase(resources.begin() + static_cast<uint32_t>(i));
        size = (std::max)(size, info.offset + info.size);
      } else if (std::holds_alternative<MatrixInfo>(resource.info)) {
        auto info = std::get<MatrixInfo>(resource.info);
        globalUBOStruct.fields.emplace_back(ResourceInfo{
            .name = resource.name,
            .info = info,
        });
        resources.erase(resources.begin() + static_cast<uint32_t>(i));
        size = (std::max)(size, info.offset + info.size);
      }
    }

    if (globalUBOStruct.fields.size() == 0) {
      return;
    }

    globals = {
        .size = size,
        .set = set,
        .binding = binding,
        .access = SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ,
        .bufferType = BufferType::Uniform,
        .info = globalUBOStruct,
    };

    hasGlobals = true;
  }
};

auto ReflectShader(Graphics::GraphicsContext &context,
                   slang::ProgramLayout *programLayout,
                   ShaderReflection &outReflection) -> Error::Error;