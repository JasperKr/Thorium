#include "descriptors.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"

namespace Graphics {

auto DescriptorHeap::Create(const GraphicsContext &context, HeapType type)
    -> Result<DescriptorHeap> {
  auto usage =
      static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
      VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT |
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  auto info = BufferCreationInfo{
      .size = size,
      .usage = usage,
      .stagingBuffer = false,
      .persistentMapping = true,
      .debugName = "Descriptor Heap",
  };

  DescriptorHeap heap{};

  auto bufferResult = Buffer::Create(context, info);
  if (Error::IsError(bufferResult)) {
    return bufferResult.error().AsUnexpected();
  }
  heap.heap = bufferResult.value();

  return heap;
}

auto DescriptorHeap::Bind(const GraphicsContext &context,
                          VkCommandBuffer buffer) const -> void {
  auto bindInfo = VkBindHeapInfoEXT{
      .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
      .heapRange =
          VkDeviceAddressRangeEXT{
              .address = heap->deviceAddress,
              .size = heap->sizeInBytes,
          },
      .reservedRangeOffset = 0,
  };

  switch (type) {
  case HeapType::Resource: {
    bindInfo.reservedRangeSize =
        context.descriptorHeapProperties.minResourceHeapReservedRange;
    vkCmdBindResourceHeapEXT(buffer, &bindInfo);
    break;
  }
  case HeapType::Sampler: {
    bindInfo.reservedRangeSize =
        context.descriptorHeapProperties.minSamplerHeapReservedRange;
    vkCmdBindSamplerHeapEXT(buffer, &bindInfo);
    break;
  }
  }
}

} // namespace Graphics