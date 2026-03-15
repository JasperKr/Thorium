#include "draw.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/error.hpp"
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>

#include <vulkan/vulkan_core.h>

namespace Graphics {

auto BindMesh(GraphicsContext &context, VkCommandBuffer cmdBuffer,
              const Mesh &mesh) -> Error {
  ZoneScoped;
  auto count =
      mesh.GetIndexCount() > 0 ? mesh.GetIndexCount() : mesh.GetVertexCount();

#ifndef NDEBUG
  switch (mesh.GetTopology()) {
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    if (count % 2 != 0) {
      return Error::Create(
          "Line List topology requires an even number of vertices.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    if (count < 2) {
      return Error::Create("Line Strip topology requires at least 2 vertices.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    if (count % 3 != 0) {
      return Error::Create("Triangle List topology requires vertex count to be "
                           "a multiple of 3.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    if (count < 3) {
      return Error::Create(
          "Triangle Strip topology requires at least 3 vertices.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
  default:
    break;
  }
#endif

  {
    auto vertexBuffer = mesh.GetVertexBuffer();
    Barrier::UpdateUsage(context, *vertexBuffer,
                         Barrier::ResourceState{
                             .stages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                             .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                         });

    std::lock_guard<std::mutex> lock(vertexBuffer->mutex);
    auto *vbo = vertexBuffer->handle;
    VkDeviceSize offset = 0;

    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vbo, &offset);
  }

  if (mesh.GetIndexCount() > 0) {
    auto indexBuffer = mesh.GetIndexBuffer();
    Barrier::UpdateUsage(context, *indexBuffer,
                         Barrier::ResourceState{
                             .stages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                             .access = VK_ACCESS_2_INDEX_READ_BIT,
                         });

    std::lock_guard<std::mutex> lock(indexBuffer->mutex);
    auto *ibo = indexBuffer->handle;

    vkCmdBindIndexBuffer(cmdBuffer, ibo, 0, mesh.GetIndexFormat());
  }

  return Error::Success();
}

auto Draw(GraphicsContext &context, Mesh &mesh, uint32_t instanceCount)
    -> Error {
  ZoneScoped;

  auto *commandBuffer = GetCommandBuffer();
  VertexFormat &format = mesh.GetVertexFormat();
  uint32_t stride = format.GetStride(0);

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  auto bindResult = BindMesh(context, commandBuffer, mesh);
  if (Error::IsError(bindResult)) {
    return bindResult;
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  DynamicRendering::SetVertexFormat(format);
  DynamicRendering::SetTopology(mesh.GetTopology());

  auto error = DynamicRendering::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  {
    ZoneScopedN("Vk Draw");
    MeshDrawRange range = mesh.GetDrawRange();

    if (mesh.GetIndexCount() > 0) {
      vkCmdDrawIndexed(commandBuffer, range.Count, instanceCount, range.Offset,
                       0, 0);
    } else {
      vkCmdDraw(commandBuffer, range.Count, instanceCount, range.Offset, 0);
    }
  }

  {
    ZoneScopedN("Resource management") {
      std::lock_guard<std::mutex> lock(mesh.GetVertexBuffer()->mutex);
      mesh.GetVertexBuffer()->MarkUse();
    }

    if (mesh.GetIndexCount() > 0) {
      std::lock_guard<std::mutex> lock(mesh.GetIndexBuffer()->mutex);
      mesh.GetIndexBuffer()->MarkUse();
    }
  }

  return Error::Success();
}

auto Dispatch(GraphicsContext &context, const Math::Uvec3 &threadgroups)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for dispatch call.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
  auto error = DynamicRendering::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  DynamicRendering::EndRendering(context);

  vkCmdDispatch(commandBuffer, threadgroups.x, threadgroups.y, threadgroups.z);

  return Error::Success();
}

auto DispatchIndirect(GraphicsContext &context,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for dispatch indirect.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
  auto error = DynamicRendering::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdDispatchIndirect(commandBuffer, indirectBuffer->handle, offset);

  return Error::Success();
}

auto DrawIndirect(GraphicsContext &context, Mesh &mesh,
                  const Ref<Buffer> &indirectBuffer,
                  VkDeviceSize offset, // NOLINT
                  uint32_t count) -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw indirect.");
  }

  auto bindResult = BindMesh(context, commandBuffer, mesh);
  if (Error::IsError(bindResult)) {
    return bindResult;
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  DynamicRendering::SetVertexFormat(mesh.GetVertexFormat());
  DynamicRendering::SetTopology(mesh.GetTopology());

  auto error = DynamicRendering::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  if (offset % 4 != 0) {
    return Error::Create(
        "Offset for vkCmdDrawIndirect must be a multiple of 4.");
  }

  vkCmdDrawIndirect(commandBuffer, indirectBuffer->handle, offset, count,
                    sizeof(VkDrawIndirectCommand));

  {
    std::lock_guard<std::mutex> lock(mesh.GetVertexBuffer()->mutex);
    mesh.GetVertexBuffer()->MarkUse();
  }
  if (mesh.GetIndexCount() > 0) {
    std::lock_guard<std::mutex> lock(mesh.GetIndexBuffer()->mutex);
    mesh.GetIndexBuffer()->MarkUse();
  }
  {
    std::lock_guard<std::mutex> lock(indirectBuffer->mutex);
    indirectBuffer->MarkUse();
  }

  return Error::Success();
}

// Vertex shader generated versions

auto Draw(GraphicsContext &context, const VkPrimitiveTopology &topology,
          uint32_t vertexCount, uint32_t instanceCount) -> Error { // NOLINT
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  DynamicRendering::SetVertexFormat({});
  DynamicRendering::SetTopology(topology);

  auto error = DynamicRendering::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);

  return Error::Success();
}

auto Draw(GraphicsContext &context, const Ref<Buffer> &indexBuffer,
          const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  DynamicRendering::SetVertexFormat({});
  DynamicRendering::SetTopology(topology);

  auto error = DynamicRendering::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  {
    Barrier::UpdateUsage(context, *indexBuffer,
                         Barrier::ResourceState{
                             .stages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                             .access = VK_ACCESS_2_INDEX_READ_BIT,
                         });

    std::lock_guard<std::mutex> lock(indexBuffer->mutex);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer->handle, 0,
                         VK_INDEX_TYPE_UINT32);
    indexBuffer->MarkUse();
  }

  vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);

  return Error::Success();
}

} // namespace Graphics