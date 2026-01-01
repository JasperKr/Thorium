#include "draw.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/rendertarget.hpp"
#include <cstdint>

namespace Graphics {
void BindMesh(VkCommandBuffer cmdBuffer, const Mesh &mesh) {
  std::vector<VkBuffer> vertexBuffers = {mesh.GetVertexBuffer()->handle};
  std::vector<VkDeviceSize> offsets = {0};
  vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(), offsets.data());
  if (mesh.GetIndexCount() > 0) {
    vkCmdBindIndexBuffer(cmdBuffer, mesh.GetIndexBuffer()->handle, 0,
                         VK_INDEX_TYPE_UINT32);
  }
}

auto Draw(GraphicsContext &context, const Mesh &mesh, uint32_t instanceCount)
    -> Error {
  RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
  auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

  BindMesh(commandBuffer, mesh);
  MeshDrawRange range = mesh.GetDrawRange();

  RenderTarget::SetVertexFormat(mesh.GetVertexFormat());
  RenderTarget::SetTopology(mesh.GetTopology());

  auto error = RenderTarget::PrepareDraw(context);
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

auto Dispatch(GraphicsContext &context, const Shader::ShaderModule &shader,
              const Math::Uvec3 &threadgroups) -> Error {
  RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
  auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

  vkCmdDispatch(commandBuffer, threadgroups.x, threadgroups.y, threadgroups.z);

  return Error::Success();
}

auto DispatchIndirect(GraphicsContext &context,
                      const Shader::ShaderModule &shader,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error {
  RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
  auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

  vkCmdDispatchIndirect(commandBuffer, indirectBuffer->handle, offset);

  return Error::Success();
}

auto DrawIndirect(GraphicsContext &context, const Mesh &mesh,
                  const Ref<Buffer> &indirectBuffer,
                  VkDeviceSize offset, // NOLINT
                  uint32_t count) -> Error {
  RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
  auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

  BindMesh(commandBuffer, mesh);

  RenderTarget::SetVertexFormat(mesh.GetVertexFormat());
  RenderTarget::SetTopology(mesh.GetTopology());

  auto error = RenderTarget::PrepareDraw(context);
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
  RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
  auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

  RenderTarget::SetVertexFormat({});
  RenderTarget::SetTopology(topology);

  auto error = RenderTarget::PrepareDraw(context);
  if (Error::IsError(error)) {
    return error;
  }

  vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);

  return Error::Success();
}

auto Draw(GraphicsContext &context, const Ref<Buffer> &indexBuffer,
          const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error {
  RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
  auto *commandBuffer = GetCommandBuffer(context, GetCurrentThreadIndex());

  RenderTarget::SetVertexFormat({});
  RenderTarget::SetTopology(topology);

  auto error = RenderTarget::PrepareDraw(context);
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