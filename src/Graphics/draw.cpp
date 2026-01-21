#include "draw.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/error.hpp"
#include <cstdint>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto BindMesh(GraphicsContext &context, VkCommandBuffer cmdBuffer,
              const Mesh &mesh) -> Error {
  auto count =
      mesh.GetIndexCount() > 0 ? mesh.GetIndexCount() : mesh.GetVertexCount();

  Barrier::UpdateUsage(context, *mesh.GetVertexBuffer(),
                       Barrier::ResourceState{
                           .stages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                           .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                       });

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

  std::vector<VkBuffer> vertexBuffers = {mesh.GetVertexBuffer()->handle};
  std::vector<VkDeviceSize> offsets = {0};

  vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(), offsets.data());

  if (mesh.GetIndexCount() > 0) {
    Barrier::UpdateUsage(context, *mesh.GetIndexBuffer(),
                         Barrier::ResourceState{
                             .stages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                             .access = VK_ACCESS_2_INDEX_READ_BIT,
                         });

    VkIndexType type{};

    switch (mesh.GetIndexFormat()) {
    case IndexFormat::None:
      return Error::Create("Unable to bind index buffer: invalid Index format");
    case IndexFormat::Uint16:
      type = VK_INDEX_TYPE_UINT16;
      break;
    case IndexFormat::Uint32:
      type = VK_INDEX_TYPE_UINT32;
      break;
    }

    vkCmdBindIndexBuffer(cmdBuffer, mesh.GetIndexBuffer()->handle, 0, type);
  }

  return Error::Success();
}

auto Draw(GraphicsContext &context, const Mesh &mesh, uint32_t instanceCount)
    -> Error {
  auto *commandBuffer = GetCommandBuffer();

  MeshDrawRange range = mesh.GetDrawRange();

  auto format = mesh.GetVertexFormat();
  auto stride = format.GetStride(0); // TODO: Multiple buffers

  auto bindResult = BindMesh(context, commandBuffer, mesh);
  if (Error::IsError(bindResult)) {
    return bindResult;
  }

  RenderTarget::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  RenderTarget::SetVertexFormat(format);
  RenderTarget::SetTopology(mesh.GetTopology());

  auto error = RenderTarget::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  if (mesh.GetIndexCount() > 0) {
    vkCmdDrawIndexed(commandBuffer, range.Count, instanceCount, range.Offset, 0,
                     0);
  } else {
    vkCmdDraw(commandBuffer, range.Count, instanceCount, range.Offset, 0);
  }

  auto timelineValue = Graphics::GetCPUTimelineSemaphoreValue(context);
  mesh.GetVertexBuffer()->MarkUse(0, timelineValue);
  mesh.GetIndexBuffer()->MarkUse(0, timelineValue);

  return Error::Success();
}

auto Dispatch(GraphicsContext &context, const Math::Uvec3 &threadgroups)
    -> Error {
  auto *commandBuffer = GetCommandBuffer();

  RenderTarget::SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
  auto error = RenderTarget::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdDispatch(commandBuffer, threadgroups.x, threadgroups.y, threadgroups.z);

  return Error::Success();
}

auto DispatchIndirect(GraphicsContext &context,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error {
  auto *commandBuffer = GetCommandBuffer();

  RenderTarget::SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
  auto error = RenderTarget::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdDispatchIndirect(commandBuffer, indirectBuffer->handle, offset);

  return Error::Success();
}

auto DrawIndirect(GraphicsContext &context, const Mesh &mesh,
                  const Ref<Buffer> &indirectBuffer,
                  VkDeviceSize offset, // NOLINT
                  uint32_t count) -> Error {
  auto *commandBuffer = GetCommandBuffer();

  auto bindResult = BindMesh(context, commandBuffer, mesh);
  if (Error::IsError(bindResult)) {
    return bindResult;
  }

  RenderTarget::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  RenderTarget::SetVertexFormat(mesh.GetVertexFormat());
  RenderTarget::SetTopology(mesh.GetTopology());

  auto error = RenderTarget::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  if (offset % 4 != 0) {
    return Error::Create(
        "Offset for vkCmdDrawIndirect must be a multiple of 4.");
  }

  vkCmdDrawIndirect(commandBuffer, indirectBuffer->handle, offset, count,
                    sizeof(VkDrawIndirectCommand));

  auto timelineValue = Graphics::GetCPUTimelineSemaphoreValue(context);
  mesh.GetVertexBuffer()->MarkUse(0, timelineValue);
  mesh.GetIndexBuffer()->MarkUse(0, timelineValue);
  indirectBuffer->MarkUse(0, timelineValue);

  return Error::Success();
}

// Vertex shader generated versions

auto Draw(GraphicsContext &context, const VkPrimitiveTopology &topology,
          uint32_t vertexCount, uint32_t instanceCount) -> Error { // NOLINT
  auto *commandBuffer = GetCommandBuffer();

  RenderTarget::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  RenderTarget::SetVertexFormat({});
  RenderTarget::SetTopology(topology);

  auto error = RenderTarget::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);

  return Error::Success();
}

auto Draw(GraphicsContext &context, const Ref<Buffer> &indexBuffer,
          const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error {
  auto *commandBuffer = GetCommandBuffer();

  RenderTarget::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  RenderTarget::SetVertexFormat({});
  RenderTarget::SetTopology(topology);

  auto error = RenderTarget::PrepareRendering(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdBindIndexBuffer(commandBuffer, indexBuffer->handle, 0,
                       VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);

  auto timelineValue = Graphics::GetCPUTimelineSemaphoreValue(context);
  indexBuffer->MarkUse(0, timelineValue);

  return Error::Success();
}

} // namespace Graphics