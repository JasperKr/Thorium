#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Graphics {
struct DescriptorHeap {
  enum class HeapType : uint8_t { Resource, Sampler };

public:
  static auto Create(const GraphicsContext &context, HeapType type)
      -> Result<DescriptorHeap>;

  auto Bind(const GraphicsContext &context, VkCommandBuffer buffer) const
      -> void;

private:
  Ref<struct Buffer> heap;
  HeapType type;
  static constexpr VkDeviceSize size = 1024UL * 8;
};

} // namespace Graphics