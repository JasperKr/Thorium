#pragma once

#include "Modules/error.hpp"
#include "hash.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <string>
#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

enum class UniformResourceType : uint8_t {
  Unknown,
  Float,
  Vector2,
  Vector3,
  Vector4,
  Matrix2x2,
  Matrix3x3,
  Matrix4x4,
  Matrix2x3,
  Matrix2x4,
  Matrix3x2,
  Matrix3x4,
  Matrix4x2,
  Matrix4x3,
  Int,
  IntVector2,
  IntVector3,
  IntVector4,
  UInt,
  UIntVector2,
  UIntVector3,
  UIntVector4,
  Bool,
  BoolVector2,
  BoolVector3,
  BoolVector4,
  Struct,
  Array,
};

struct ShaderResource {
  std::string name;

  uint32_t set;
  uint32_t binding;

  // For uniform buffers and storage buffers
  uint32_t offset;
  slang::TypeLayoutReflection *typeLayout;

  VkShaderStageFlagBits stage;
  VkDescriptorType type;

  uint32_t count;

  auto operator==(const ShaderResource &other) const -> bool {
    return name == other.name && set == other.set && binding == other.binding &&
           stage == other.stage && type == other.type && count == other.count;
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
    hasher.add(std::hash<uint32_t>()(resource.count));
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

struct ShaderReflection {
  std::vector<ShaderResource> resources;
  std::vector<PushConstantResource> pushConstants;

  std::vector<VkDescriptorSetLayout> setLayouts;
};

auto ReflectShader(slang::ProgramLayout *programLayout,
                   ShaderReflection &outReflection) -> Error::Error;