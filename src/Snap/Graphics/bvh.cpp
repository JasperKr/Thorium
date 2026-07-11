#include "bvh.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
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

  BvhScratchBuffer = CHECK_RES(Buffer::Create(context, info));

  return {};
}

auto DeInitializeBVHModule() -> void { BvhScratchBuffer.reset(); }

auto BLAS::Create(const GraphicsContext &context, const Mesh &mesh)
    -> Result<Ref<BLAS>> {
  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

  auto positionAddress = CHECK_RES(mesh.GetVertexBuffer()->GetDeviceAddress());
  auto indexAddress = CHECK_RES(mesh.GetIndexBuffer()->GetDeviceAddress());

  geometry.geometry.triangles = {
      .sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData =
          {
              .deviceAddress = positionAddress,
          },
      .vertexStride = sizeof(float) * 3,
      .maxVertex = mesh.GetVertexCount() - 1,
      .indexType = VK_INDEX_TYPE_UINT32,
      .indexData =
          {
              .deviceAddress = indexAddress,
          },
  };

  uint32_t primitiveCount = mesh.GetIndexCount() / 3;

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  // Query required sizes
  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildInfo, &primitiveCount, &sizeInfo);

  auto bvh = Ref<BLAS>::Make();

  // Create AS storage buffer
  bvh->accelerationStructureBuffer = CHECK_RES(Buffer::Create(
      context,
      {
          .size = sizeInfo.accelerationStructureSize,
          .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .stagingBuffer = false,
          .persistentMapping = false,
          .debugName = "BLAS Buffer",
      }));

  // Create VkAccelerationStructureKHR
  VkAccelerationStructureCreateInfoKHR asCreateInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = bvh->accelerationStructureBuffer->handle,
      .size = sizeInfo.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
  };

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    CHECK_NEW_ERR(vkCreateAccelerationStructureKHR(
        context.device, &asCreateInfo, GetAllocationCallbacks(),
        &bvh->accelerationStructure));
  }

  ERR_ASSERT(BvhScratchBuffer != nullptr);
  ERR_ASSERT(BvhScratchBuffer->size >= sizeInfo.buildScratchSize);

  auto scratchAddress = CHECK_RES(BvhScratchBuffer->GetDeviceAddress());

  // Finalize build info
  buildInfo.dstAccelerationStructure = bvh->accelerationStructure;
  buildInfo.scratchData.deviceAddress = scratchAddress;

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = primitiveCount,
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };

  const VkAccelerationStructureBuildRangeInfoKHR *rangePtr = &range;

  DynamicRendering::EndRendering(context);

  auto *cmdBuffer = GetCommandBuffer();
  ERR_ASSERT(cmdBuffer != nullptr);

  BvhScratchBuffer->MarkUse();
  Barrier::UpdateUsage(
      context, *BvhScratchBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR});

  Barrier::UpdateUsage(
      context, *bvh->accelerationStructureBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR});

  vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);

  Barrier::UpdateUsage(
      context, *bvh->accelerationStructureBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR});

  Barrier::UpdateUsage(
      context, *BvhScratchBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR});

  bvh->accelerationStructureBuffer->MarkUse();

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = bvh->accelerationStructure,
  };

  bvh->accelerationStructureAddress =
      vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);

  return bvh;
}

auto BLAS::GetDeviceAddress() const -> VkDeviceAddress {
  return accelerationStructureAddress;
}

auto TLAS::GetDeviceAddress() const -> VkDeviceAddress {
  return accelerationStructureAddress;
}

auto TLAS::Create(const GraphicsContext &context) -> Result<Ref<TLAS>> {
  auto tlas = Ref<TLAS>::Make();

  // Filled when adding instances
  std::vector<VkAccelerationStructureInstanceKHR> instances;

  // Example:
  // instances.push_back({
  //     .transform = ...,
  //     .instanceCustomIndex = objectID,
  //     .mask = 0xFF,
  //     .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
  //     .accelerationStructureReference = blasAddress,
  // });

  // Upload instances to GPU
  tlas->instanceBuffer = CHECK_RES(Buffer::Create(
      context,
      {
          .size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
          .usage =
              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .stagingBuffer = true,
          .persistentMapping = false,
          .debugName = "TLAS Instances",
      }));

  auto instanceAddress = CHECK_RES(tlas->instanceBuffer->GetDeviceAddress());

  VkAccelerationStructureGeometryKHR geometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
  };

  geometry.geometry.instances = {
      .sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .arrayOfPointers = VK_FALSE,
      .data =
          {
              .deviceAddress = instanceAddress,
          },
  };

  auto instanceCount = static_cast<uint32_t>(instances.size());

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildInfo, &instanceCount, &sizeInfo);

  // Create AS storage buffer
  tlas->accelerationStructureBuffer = CHECK_RES(Buffer::Create(
      context,
      {
          .size = sizeInfo.accelerationStructureSize,
          .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .stagingBuffer = false,
          .persistentMapping = false,
          .debugName = "TLAS Buffer",
      }));

  // Create VkAccelerationStructureKHR
  VkAccelerationStructureCreateInfoKHR asCreateInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = tlas->accelerationStructureBuffer->handle,
      .size = sizeInfo.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
  };

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    CHECK_NEW_ERR(vkCreateAccelerationStructureKHR(
        context.device, &asCreateInfo, GetAllocationCallbacks(),
        &tlas->accelerationStructure));
  }

  ERR_ASSERT(BvhScratchBuffer != nullptr);
  ERR_ASSERT(BvhScratchBuffer->size >= sizeInfo.buildScratchSize);

  auto scratchAddress = CHECK_RES(BvhScratchBuffer->GetDeviceAddress());

  // Finalize build info

  buildInfo.dstAccelerationStructure = tlas->accelerationStructure;
  buildInfo.scratchData.deviceAddress = scratchAddress;

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = instanceCount,
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };

  const VkAccelerationStructureBuildRangeInfoKHR *rangePtr = &range;

  DynamicRendering::EndRendering(context);

  auto *cmdBuffer = GetCommandBuffer();
  ERR_ASSERT(cmdBuffer != nullptr);

  BvhScratchBuffer->MarkUse();
  Barrier::UpdateUsage(
      context, *BvhScratchBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR});

  Barrier::UpdateUsage(
      context, *tlas->accelerationStructureBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR});

  vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);

  Barrier::UpdateUsage(
      context, *tlas->accelerationStructureBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR});

  Barrier::UpdateUsage(
      context, *BvhScratchBuffer,
      {.stages = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
       .access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR});

  tlas->accelerationStructureBuffer->MarkUse();

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = tlas->accelerationStructure,
  };

  tlas->accelerationStructureAddress =
      vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);

  instanceCount = static_cast<uint32_t>(instances.size());

  return tlas;
}

auto TLAS::AddInstance(const Ref<BLAS> &blas, const Math::Matrix4x4 &transform)
    -> uint32_t {
  VkAccelerationStructureInstanceKHR instance{};

  // clang-format off
  instance.transform = VkTransformMatrixKHR{
      transform.At(0, 0), transform.At(1, 0), transform.At(2, 0), transform.At(3, 0),
      transform.At(0, 1), transform.At(1, 1), transform.At(2, 1), transform.At(3, 1),
      transform.At(0, 2), transform.At(1, 2), transform.At(2, 2), transform.At(3, 2),
  };
  // clang-format on

  instance.instanceCustomIndex = static_cast<uint32_t>(instances.size());
  instance.mask = 0xFF; // All bits enabled NOLINT
  instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  instance.accelerationStructureReference = blas->GetDeviceAddress();

  instances.emplace_back(instance);

  return static_cast<uint32_t>(instances.size() - 1);
}

auto TLAS::RemoveInstance(uint32_t index) -> void {
  if (index >= instances.size()) {
    return;
  }

  instances.erase(instances.begin() + index);
}

auto TLAS::UpdateInstance(uint32_t index, const Math::Matrix4x4 &transform)
    -> void {
  if (index >= instances.size()) {
    return;
  }

  VkAccelerationStructureInstanceKHR &instance = instances.at(index);

  // clang-format off
  instance.transform = VkTransformMatrixKHR{
      transform.At(0, 0), transform.At(1, 0), transform.At(2, 0), transform.At(3, 0),
      transform.At(0, 1), transform.At(1, 1), transform.At(2, 1), transform.At(3, 1),
      transform.At(0, 2), transform.At(1, 2), transform.At(2, 2), transform.At(3, 2),
  };
  // clang-format on
}

auto TLAS::Refit(const GraphicsContext &context) -> Error {
  ERR_ASSERT(BvhScratchBuffer != nullptr);
  ERR_ASSERT(BvhScratchBuffer->size >= InitialScratchBufferSize);
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(instanceBuffer != nullptr);
  ERR_ASSERT(instances.size() > 0);

  [[unlikely]]
  if (instances.size() != instanceCount) {
    PrintWarning(
        "Refit called on TLAS with a different number of instances than "
        "originally built. Refit is only valid if the number of instances "
        "remains the same.");
  }

  auto instanceAddress = CHECK_RES(instanceBuffer->GetDeviceAddress());

  VkAccelerationStructureGeometryKHR geometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry = {
          .instances = {
              .sType =
                  VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
              .arrayOfPointers = VK_FALSE,
              .data = {.deviceAddress = instanceAddress},
          }}};

  auto scratchAddress = CHECK_RES(BvhScratchBuffer->GetDeviceAddress());

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
      .srcAccelerationStructure = accelerationStructure,
      .dstAccelerationStructure = accelerationStructure,
      .geometryCount = 1,
      .pGeometries = &geometry,
      .scratchData = {.deviceAddress = scratchAddress},
  };

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = static_cast<uint32_t>(instances.size()),
  };

  const auto *rangePtr = &range;

  vkCmdBuildAccelerationStructuresKHR(GetCommandBuffer(), 1, &buildInfo,
                                      &rangePtr);

  return Error::Success();
}

auto TLAS::Rebuild(const GraphicsContext &context) -> Error {
  ERR_ASSERT(BvhScratchBuffer != nullptr);
  ERR_ASSERT(BvhScratchBuffer->size >= InitialScratchBufferSize);
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(instanceBuffer != nullptr);
  ERR_ASSERT(instances.size() > 0);

  auto instanceAddress = CHECK_RES(instanceBuffer->GetDeviceAddress());

  VkAccelerationStructureGeometryKHR geometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry = {
          .instances = {
              .sType =
                  VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
              .arrayOfPointers = VK_FALSE,
              .data = {
                  .deviceAddress = instanceAddress,
              }}}};

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .dstAccelerationStructure = accelerationStructure,
      .geometryCount = 1,
      .pGeometries = &geometry,
      .scratchData = {.deviceAddress =
                          CHECK_RES(BvhScratchBuffer->GetDeviceAddress())},
  };

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = static_cast<uint32_t>(instances.size()),
  };

  const auto *rangePtr = &range;

  vkCmdBuildAccelerationStructuresKHR(GetCommandBuffer(), 1, &buildInfo,
                                      &rangePtr);

  return Error::Success();
}

} // namespace Graphics