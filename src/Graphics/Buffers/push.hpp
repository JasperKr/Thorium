#pragma once

#include <cstddef>
#include <utility>

#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"
#include "tl/expected.hpp"

namespace Graphics {
struct FlushInfo {
  VkCommandBuffer commandBuffer;
  VkPipelineLayout pipelineLayout;
  VkShaderStageFlags stageFlags;
};

struct PushBuffer {
public:
  explicit PushBuffer(BufferInfo layout) : layout(std::move(layout)) {
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
  [[nodiscard]] auto GetBufferSize() const -> size_t { return layout.size; }
  auto FlushData(FlushInfo &info) -> void;

  template <typename T>
  auto SetData(const std::string &name, const T &value) -> Error::Error {
    if (layout.type != BufferResourceType::Struct) {
      return Error::Error(
          "SetData with name only supported for struct push buffers");
    }

    size_t dataSize = sizeof(T);
    auto result = OffsetOf(name);

    if (Error::IsError(result)) {
      return result.error();
    }

    size_t offset = result.value();

    if (offset + dataSize > data.size()) {
      return Error::Error("Data exceeds buffer size");
    }

    // NOLINTNEXTLINE
    std::memcpy(data.data() + offset, &value, dataSize);
  }

  template <typename T> auto SetData(const T &value) -> Error::Error {
    if (layout.type != BufferResourceType::Scalar &&
        layout.type != BufferResourceType::Vector &&
        layout.type != BufferResourceType::Matrix) {
      return Error::Error("SetData without name only supported for scalar, "
                          "vector, and matrix push buffers");
    }

    size_t dataSize = sizeof(T);
    if (dataSize > data.size()) {
      return Error::Error("Data exceeds buffer size");
    }

    // NOLINTNEXTLINE
    std::memcpy(data.data(), &value, dataSize);
  }

private:
  auto OffsetOf(const std::string &name) const
      -> tl::expected<size_t, Error::Error>;
  BufferInfo layout;
  std::vector<uint8_t> data;
};

auto CreatePushBuffer(const BufferInfo &layout)
    -> tl::expected<PushBuffer, Error::Error>;

} // namespace Graphics