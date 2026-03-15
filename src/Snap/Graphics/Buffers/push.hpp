#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"

namespace Graphics {
struct FlushInfo {
  VkCommandBuffer commandBuffer;
  VkPipelineLayout pipelineLayout;
};

struct PushBuffer {
public:
  explicit PushBuffer(const ResourceInfo &layout,
                      VkShaderStageFlags stage = VK_SHADER_STAGE_ALL);

  [[nodiscard]] auto GetBufferOffset() const -> size_t;
  [[nodiscard]] auto GetBufferSize() const -> size_t;
  [[nodiscard]] auto GetLayout() const -> const ResourceInfo &;
  auto FlushData(FlushInfo &info) -> void;

  auto SetData(const ResourceKey &key, const std::span<const uint8_t> &values)
      -> Error;

  auto SetData(const std::span<const uint8_t> &values) -> Error;

  [[nodiscard]] auto GetStageFlags() const -> VkShaderStageFlags;

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