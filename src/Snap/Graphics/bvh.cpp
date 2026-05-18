#include "bvh.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Graphics {

Ref<Buffer> BvhScratchBuffer; // NOLINT

auto InitializeBVHModule(const GraphicsContext &context) -> Error {
  auto info = BufferCreationInfo{
      .size = InitialScratchBufferSize,
      .usage = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .stagingBuffer = false,
      .persistentMapping = false,
      .debugName = "BVH Scratch Buffer",
  };

  auto result = Buffer::Create(context, info);

  if (!result) {
    return result.error();
  }

  BvhScratchBuffer = result.value();

  return {};
}

auto DeInitializeBVHModule() -> void { BvhScratchBuffer.reset(); }

} // namespace Graphics