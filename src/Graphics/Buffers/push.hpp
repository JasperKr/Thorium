#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
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
    if (std::holds_alternative<ScalarInfo>(layout.info)) {
      data.resize(std::get<ScalarInfo>(layout.info).size);
    } else if (std::holds_alternative<VectorInfo>(layout.info)) {
      data.resize(std::get<VectorInfo>(layout.info).size);
    } else if (std::holds_alternative<MatrixInfo>(layout.info)) {
      data.resize(std::get<MatrixInfo>(layout.info).size);
    } else if (std::holds_alternative<StructInfo>(layout.info)) {
      data.resize(std::get<StructInfo>(layout.info).size);
    }
  }

  [[nodiscard]] auto GetBufferOffset() const -> size_t { return layout.offset; }
  [[nodiscard]] auto GetBufferSize() const -> size_t {
    if (std::holds_alternative<StructInfo>(layout.info)) {
      return std::get<StructInfo>(layout.info).size;
    }
    return layout.size;
  }
  [[nodiscard]] auto GetLayout() const -> const BufferInfo & { return layout; }
  auto FlushData(FlushInfo &info) -> void;

  auto SetData(const std::string &name, const std::span<const uint8_t> &values)
      -> Error::Error {

    if (!std::holds_alternative<StructInfo>(layout.info)) {
      return Error::Create(
          "SetData with name only supported for struct push buffers");
    }

    auto result = InfoOf(name);

    if (Error::IsError(result)) {
      return result.error();
    }

    size_t offset = result.value().offset;

    if (result.value().GetSize() != values.size()) {
      return Error::Create("Data size does not match field size");
    }

    if (offset + values.size() > data.size()) {
      return Error::Create("Data exceeds buffer size");
    }

    // NOLINTNEXTLINE
    std::memcpy(data.data() + offset, values.data(), values.size());

    return Error::Success();
  }

  auto SetData(const std::span<const uint8_t> &values) -> Error::Error {
    if (!layout.IsScalar() && !layout.IsVector() && !layout.IsMatrix()) {
      return Error::Create("SetData without name only supported for scalar, "
                           "vector, and matrix push buffers");
    }

    if (values.size() > data.size()) {
      return Error::Create("Data exceeds buffer size");
    }

    std::memcpy(data.data(), values.data(), values.size());

    return Error::Success();
  }

  [[nodiscard]] auto GetStageFlags() const -> VkShaderStageFlags {
    return stageFlags;
  }

  auto InfoOf(const std::string &name) const
      -> tl::expected<ResourceInfo, Error::Error>;

private:
  BufferInfo layout;
  std::vector<uint8_t> data;
  VkShaderStageFlags stageFlags{VK_SHADER_STAGE_ALL};
};

} // namespace Graphics