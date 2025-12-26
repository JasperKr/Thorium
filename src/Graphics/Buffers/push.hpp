#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Graphics/reflect.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"

namespace Graphics {
struct FlushInfo {
  VkCommandBuffer commandBuffer;
  VkPipelineLayout pipelineLayout;
};

struct PushBuffer {
public:
  explicit PushBuffer(const ResourceInfo &layout,
                      VkShaderStageFlags stage = VK_SHADER_STAGE_ALL)
      : layout(layout), stageFlags(stage) {

    if (!layout.IsBuffer()) {
      PrintError("PushBuffer layout must be a buffer.");
      return;
    }

    const auto &bufferInfo = std::get<BufferInfo>(layout.info);

    if (std::holds_alternative<ScalarInfo>(bufferInfo.info)) {
      data.resize(std::get<ScalarInfo>(bufferInfo.info).size);
    } else if (std::holds_alternative<VectorInfo>(bufferInfo.info)) {
      data.resize(std::get<VectorInfo>(bufferInfo.info).size);
    } else if (std::holds_alternative<MatrixInfo>(bufferInfo.info)) {
      data.resize(std::get<MatrixInfo>(bufferInfo.info).size);
    } else if (std::holds_alternative<StructInfo>(bufferInfo.info)) {
      data.resize(std::get<StructInfo>(bufferInfo.info).size);
    }
  }

  [[nodiscard]] auto GetBufferOffset() const -> size_t { return layout.offset; }
  [[nodiscard]] auto GetBufferSize() const -> size_t {
    const auto &bufferInfo = std::get<BufferInfo>(layout.info);
    if (std::holds_alternative<StructInfo>(bufferInfo.info)) {
      return std::get<StructInfo>(bufferInfo.info).size;
    }
    return bufferInfo.size;
  }
  [[nodiscard]] auto GetLayout() const -> const ResourceInfo & {
    return layout;
  }
  auto FlushData(FlushInfo &info) -> void;

  auto SetData(const ResourceKey &key, const std::span<const uint8_t> &values)
      -> Error::Error {

    if (!std::holds_alternative<StructInfo>(layout.info)) {
      return Error::Create(
          "SetData with key only supported for struct push buffers");
    }

    const auto *result = GetUniform(key.begin(), key.end());
    if (result == nullptr) {
      return Error::Create("Uniform not found in push buffer.");
    }

    size_t offset = result->offset;

    if (result->GetSize() != values.size()) {
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

  [[nodiscard]] auto ContainsUniform(ResourceKey::const_iterator iterator,
                                     ResourceKey::const_iterator end) const
      -> bool;
  [[nodiscard]] auto GetUniform(ResourceKey::const_iterator iterator,
                                ResourceKey::const_iterator end) const
      -> const ResourceInfo *;

private:
  ResourceInfo layout;
  std::vector<uint8_t> data;
  VkShaderStageFlags stageFlags{VK_SHADER_STAGE_ALL};
};

} // namespace Graphics