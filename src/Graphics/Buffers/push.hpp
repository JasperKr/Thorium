#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include "tl/expected.hpp"

namespace Graphics {
struct FlushInfo {
  VkCommandBuffer commandBuffer;
  VkPipelineLayout pipelineLayout;
};

struct PushBuffer {
public:
  explicit PushBuffer(const BufferInfo &layout,
                      VkShaderStageFlags stage = VK_SHADER_STAGE_ALL)
      : layout(layout), stageFlags(stage) {
    switch (layout.type) {
    case BufferResourceType::Scalar:
      data.resize(std::get<ScalarInfo>(layout.info).size);
      break;
    case BufferResourceType::Vector:
      data.resize(std::get<VectorInfo>(layout.info).size);
      break;
    case BufferResourceType::Matrix:
      data.resize(std::get<MatrixInfo>(layout.info).size);
      break;
    case BufferResourceType::Struct:
      data.resize(std::get<StructInfo>(layout.info).size);
      break;
    default:
      throw std::runtime_error("Unsupported PushBuffer layout type");
    }
  }

  [[nodiscard]] auto GetBufferOffset() const -> size_t { return layout.offset; }
  auto GetBufferSize() const -> size_t {
    if (layout.type == BufferResourceType::Struct) {
      return std::get<StructInfo>(layout.info).size;
    }
    return layout.size;
  }
  [[nodiscard]] auto GetLayout() const -> const BufferInfo & { return layout; }
  auto FlushData(FlushInfo &info) -> void;

  template <typename T>
  auto SetData(const std::string &name, const std::span<T> &value)
      -> Error::Error {

    static_assert(std::is_same_v<T, float> || std::is_same_v<T, uint8_t> ||
                  std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> ||
                  std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                  std::is_same_v<T, int32_t>);

    if (layout.type != BufferResourceType::Struct) {
      return Error::Create(
          "SetData with name only supported for struct push buffers");
    }

    size_t dataSize = sizeof(T) * value.size();
    auto result = InfoOf(name);

    if (Error::IsError(result)) {
      return result.error();
    }

    size_t offset = result.value().GetOffset();

    if (result.value().GetSize() != dataSize) {
      return Error::Create("Data size does not match field size");
    }

    if (offset + dataSize > data.size()) {
      return Error::Create("Data exceeds buffer size");
    }

    // NOLINTNEXTLINE
    std::memcpy(data.data() + offset, value.data(), dataSize);
  }

  template <typename T>
  auto SetData(const std::span<T> &value) -> Error::Error {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, uint8_t> ||
                  std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> ||
                  std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                  std::is_same_v<T, int32_t>);

    if (layout.type != BufferResourceType::Scalar &&
        layout.type != BufferResourceType::Vector &&
        layout.type != BufferResourceType::Matrix) {
      return Error::Create("SetData without name only supported for scalar, "
                           "vector, and matrix push buffers");
    }

    size_t dataSize = sizeof(T) * value.size();

    if (dataSize > data.size()) {
      return Error::Create("Data exceeds buffer size");
    }

    std::memcpy(data.data(), value.data(), dataSize);
    return Error::Success();
  }

  template <typename T>
  auto SetData(const std::string &name, const std::vector<T> &value)
      -> Error::Error {
    if (layout.type != BufferResourceType::Struct) {
      return Error::Create(
          "SetData with name only supported for struct push buffers");
    }

    size_t dataSize = sizeof(T);
    auto result = InfoOf(name);

    if (Error::IsError(result)) {
      return result.error();
    }

    size_t offset = result.value().GetOffset();

    if (offset + dataSize > data.size()) {
      return Error::Create("Data exceeds buffer size");
    }

    // NOLINTNEXTLINE
    std::memcpy(data.data() + offset, value.data(), dataSize);
  }

  template <typename T>
  auto SetData(const std::vector<T> &value) -> Error::Error {
    if (layout.type != BufferResourceType::Scalar &&
        layout.type != BufferResourceType::Vector &&
        layout.type != BufferResourceType::Matrix) {
      return Error::Create("SetData without name only supported for scalar, "
                           "vector, and matrix push buffers");
    }

    size_t dataSize = sizeof(T);
    if (dataSize > data.size()) {
      return Error::Create("Data exceeds buffer size");
    }

    std::memcpy(data.data(), value.data(), dataSize);

    return Error::Success();
  }

  auto GetStageFlags() const -> VkShaderStageFlags { return stageFlags; }

  auto InfoOf(const std::string &name) const
      -> tl::expected<StructFieldInfo, Error::Error>;

private:
  BufferInfo layout;
  std::vector<uint8_t> data;
  VkShaderStageFlags stageFlags{VK_SHADER_STAGE_ALL};
};

} // namespace Graphics