#include "draw.hpp"
#include "Graphics/barrier.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/snapshot.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>

#include <vulkan/vulkan_core.h>

namespace Graphics {

using namespace Snapshot;

auto BindMesh(const GraphicsContext &context, VkCommandBuffer cmdBuffer,
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
  auto &threadContext = GetThreadContext();

  auto vertexBuffer = mesh.GetVertexBuffer();
  if (!vertexBuffer.isValid()) {
    return Error::Create("Mesh has no vertex buffer.");
  }

  Barrier::UpdateUsage(context, *vertexBuffer,
                       Barrier::ResourceState{
                           .stages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                           .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                       });
  if (mesh.GetIndexCount() > 0) {
    auto indexBuffer = mesh.GetIndexBuffer();
    if (!indexBuffer.isValid()) {
      return Error::Create("Mesh has index count but no index buffer.");
    }

    Barrier::UpdateUsage(context, *indexBuffer,
                         Barrier::ResourceState{
                             .stages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                             .access = VK_ACCESS_2_INDEX_READ_BIT,
                         });

    if (threadContext.currentIndexBuffer != indexBuffer->handle) {
      std::lock_guard<std::mutex> lock(indexBuffer->mutex);

      vkCmdBindIndexBuffer(cmdBuffer, indexBuffer->handle, 0,
                           mesh.GetIndexFormat());
      threadContext.currentIndexBuffer = indexBuffer->handle;
    }
  }

  if (threadContext.currentVertexBuffer != vertexBuffer->handle) {
    std::lock_guard<std::mutex> lock(vertexBuffer->mutex);
    VkDeviceSize offset = 0;

    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer->handle, &offset);
    threadContext.currentVertexBuffer = vertexBuffer->handle;
  }

  return Error::Success();
}

inline auto UpdateShaderResourceLayouts(const GraphicsContext &context,
                                        VkPipelineStageFlags2 stage) -> Error {
  auto shader = DynamicRendering::GetShader();
  if (shader == nullptr) {
    return Error::Success(); // No shader, so nothing to update
  }
  const auto &boundTextures = shader->GetState().userBoundTextures;

  for (const auto &resource : boundTextures) {
    CHECK_ERR(resource.second.first->UseAsSampler(context, stage));
  }

  return Error::Success();
}

inline auto InsertResourceBarriers(const GraphicsContext &context) -> Error {
  auto shader = DynamicRendering::GetShader();
  if (shader == nullptr) {
    return Error::Success(); // No shader, so nothing to update
  }

  for (auto &texturePair : shader->GetState().userBoundTextures) {
    auto &texture = texturePair.second;
    auto key = texturePair.first;

    const auto *infoResult = shader->GetSlotDescription(key);
    if (infoResult == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound texture slot.");
    }

    const auto &info = infoResult->GetInfo<Reflect::SamplerInfo>();

    VkAccessFlags2 access = 0;

    switch (info.access) {
    case SLANG_RESOURCE_ACCESS_READ:
      access = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_READ_WRITE:
      access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_WRITE:
      access = VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    default:
      break;
    }

    if (access == 0) {
      PrintWarning("Texture access type is Unknown for slang access: {}, "
                   "skipping barrier.",
                   static_cast<uint32_t>(info.access));
      continue;
    }

    auto stages = VK_PIPELINE_STAGE_2_NONE;

    for (const auto &stage : shader->entryPoints) {
      switch (stage.second) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        stages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
        stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        break;
      case VK_SHADER_STAGE_COMPUTE_BIT:
        stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        break;
      default:
        break;
      }
    }

#ifndef NDEBUG
    const auto &targets = DynamicRendering::GetRenderTargets();
    for (const auto &target : targets) {
      if (target.texture == texture.first) {
        auto debugname = texture.first->GetDebugName();
        return Error::Createf(
            "Texture {} is bound as a render target, but is also used as a "
            "shader "
            "resource. This is not supported and may indicate a bug in the "
            "application.",
            debugname);
      }
    }
#endif

    Barrier::UpdateUsage(context, *texture.first,
                         {
                             .stages = stages,
                             .access = access,
                         });
  }

  return Error::Success();
}

auto Draw(const GraphicsContext &context, Mesh &mesh, uint32_t instanceCount)
    -> Error {
  ZoneScoped;

  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  CHECK_ERR(BindMesh(context, commandBuffer, mesh));

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  DynamicRendering::SetTopology(mesh.GetTopology());

  auto &vertexFormat = mesh.GetVertexFormat();
  vertexFormat.BindDynamicInputState(commandBuffer);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  CHECK_ERR(DynamicRendering::PrepareRendering(context));

  auto vertexCount = mesh.GetVertexCount();

  {
    ZoneScopedN("Vk Draw");
    MeshDrawRange range = mesh.GetDrawRange();

    if (mesh.GetIndexCount() > 0) {
      vkCmdDrawIndexed(commandBuffer, range.Count, instanceCount, range.Offset,
                       0, 0);

#if Enable_Snapshots
      CaptureEvent(DrawIndexedEvent(mesh.GetIndexCount(), instanceCount,
                                    range.Offset, 0, 0));
#endif
    } else {
      vkCmdDraw(commandBuffer, range.Count, instanceCount, range.Offset, 0);

#if Enable_Snapshots
      CaptureEvent(DrawEvent(vertexCount, instanceCount, 0, 0));
#endif
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

auto Dispatch(const GraphicsContext &context, const Math::Uvec3 &threadgroups)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for dispatch call.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
  CHECK_ERR(DynamicRendering::PrepareRendering(context));
  DynamicRendering::EndRendering(context);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  vkCmdDispatch(commandBuffer, threadgroups.x, threadgroups.y, threadgroups.z);

#if Enable_Snapshots
  CaptureEvent(DispatchEvent(threadgroups.x, threadgroups.y, threadgroups.z));
#endif

  return Error::Success();
}

auto DispatchIndirect(const GraphicsContext &context,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for dispatch indirect.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
  CHECK_ERR(DynamicRendering::PrepareRendering(context));
  DynamicRendering::EndRendering(context);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  vkCmdDispatchIndirect(commandBuffer, indirectBuffer->handle, offset);

#if Enable_Snapshots
  CaptureEvent(DispatchIndirectEvent(indirectBuffer->handle, offset));
#endif

  return Error::Success();
}

auto DrawIndirect(const GraphicsContext &context, Mesh &mesh,
                  const Ref<Buffer> &indirectBuffer,
                  VkDeviceSize offset, // NOLINT
                  uint32_t count) -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw indirect.");
  }

  CHECK_ERR(BindMesh(context, commandBuffer, mesh));

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  auto vertexFormat = mesh.GetVertexFormat();
  vertexFormat.BindDynamicInputState(commandBuffer);
  DynamicRendering::SetTopology(mesh.GetTopology());

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  CHECK_ERR(DynamicRendering::PrepareRendering(context));

  if (offset % 4 != 0) {
    return Error::Create(
        "Offset for vkCmdDrawIndirect must be a multiple of 4.");
  }

  vkCmdDrawIndirect(commandBuffer, indirectBuffer->handle, offset, count,
                    sizeof(VkDrawIndirectCommand));

#if Enable_Snapshots
  CaptureEvent(DrawIndirectEvent(indirectBuffer->handle, offset, count,
                                 sizeof(VkDrawIndirectCommand)));
#endif

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

auto Draw(const GraphicsContext &context, const VkPrimitiveTopology &topology,
          uint32_t vertexCount, uint32_t instanceCount) -> Error { // NOLINT
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  vkCmdSetVertexInputEXT(commandBuffer, 0, nullptr, 0, nullptr);
  DynamicRendering::SetTopology(topology);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  CHECK_ERR(DynamicRendering::PrepareRendering(context));

  vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);

#if Enable_Snapshots
  CaptureEvent(DrawEvent(vertexCount, instanceCount, 0, 0));
#endif

  return Error::Success();
}

auto Draw(const GraphicsContext &context, const Ref<Buffer> &indexBuffer,
          const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  DynamicRendering::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  vkCmdSetVertexInputEXT(commandBuffer, 0, nullptr, 0, nullptr);
  DynamicRendering::SetTopology(topology);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  CHECK_ERR(DynamicRendering::PrepareRendering(context));

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

#if Enable_Snapshots
  CaptureEvent(DrawIndexedEvent(indexCount, instanceCount, 0, 0, 0));
#endif

  return Error::Success();
}

} // namespace Graphics