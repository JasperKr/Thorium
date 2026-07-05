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
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>

#include <vulkan/vulkan_core.h>

namespace Graphics {

// NOLINTNEXTLINE
thread_local Ref<Mesh> QuadMesh;

struct FormatDefault2D {
  float position[2]; // NOLINT
  float texCoord[2]; // NOLINT
  uint32_t color;
};

auto CreateQuad01Mesh(const Graphics::GraphicsContext &context)
    -> Result<Ref<Graphics::Mesh>> {
  std::vector<FormatDefault2D> vertices{4};
  std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

  vertices[0].position[0] = 0.0F;
  vertices[0].position[1] = 0.0F;
  vertices[0].texCoord[0] = 0.0F;
  vertices[0].texCoord[1] = 0.0F;
  vertices[0].color = ~0U;

  vertices[1].position[0] = 1.0F;
  vertices[1].position[1] = 0.0F;
  vertices[1].texCoord[0] = 1.0F;
  vertices[1].texCoord[1] = 0.0F;
  vertices[1].color = ~0U;

  vertices[2].position[0] = 1.0F;
  vertices[2].position[1] = 1.0F;
  vertices[2].texCoord[0] = 1.0F;
  vertices[2].texCoord[1] = 1.0F;
  vertices[2].color = ~0U;

  vertices[3].position[0] = 0.0F;
  vertices[3].position[1] = 1.0F;
  vertices[3].texCoord[0] = 0.0F;
  vertices[3].texCoord[1] = 1.0F;
  vertices[3].color = ~0U;

  static Graphics::VertexFormat vertexFormat({
      Graphics::VertexComponent{
          .name = "Position",
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      Graphics::VertexComponent{
          .name = "TexCoord",
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      Graphics::VertexComponent{
          .name = "Color",
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R8G8B8A8_UNORM,
      },
  });

  // NOLINTNEXTLINE; Reinterpret cast is necessary here
  auto span = std::span<uint8_t>(reinterpret_cast<uint8_t *>(vertices.data()),
                                 vertexFormat.GetBindings()[0].stride *
                                     vertices.size());

  assert(sizeof(FormatDefault2D) == vertexFormat.GetBindings()[0].stride);

  auto mesh = CHECK_RES(Graphics::Mesh::Create(context, vertexFormat, {span}));

  CHECK_ERR(mesh->SetVertices(context, 0, span));

  auto indexSpan = std::span<uint8_t>( // NOLINTNEXTLINE
      reinterpret_cast<uint8_t *>(indices.data()),
      indices.size() * Graphics::GetIndexFormatSize(VK_INDEX_TYPE_UINT32));

  CHECK_ERR(mesh->SetIndices(context, indexSpan, VK_INDEX_TYPE_UINT32));

  return mesh;
}

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

    std::lock_guard<std::mutex> lock(indexBuffer->mutex);

    if (indexBuffer->handle == VK_NULL_HANDLE) {
      return Error::Create("Index buffer handle is null.");
    }

    if (threadContext.currentMesh != mesh.getID()) {
      vkCmdBindIndexBuffer(cmdBuffer, indexBuffer->handle, 0,
                           mesh.GetIndexFormat());
    }
  }

  if (threadContext.currentMesh != mesh.getID()) {
    auto bindings = mesh.GetBindingRanges();
    for (const auto &binding : bindings) {
      vkCmdBindVertexBuffers(cmdBuffer, binding.firstBinding,
                             binding.bindingCount, binding.bindings,
                             binding.offsets);
    }

    threadContext.currentMesh = mesh.getID();
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
    switch (resource.second.second->access) {
    case SLANG_RESOURCE_ACCESS_READ:
      CHECK_ERR(resource.second.first->UseAsSampler(context, stage));
      break;
    case SLANG_RESOURCE_ACCESS_READ_WRITE:
    case SLANG_RESOURCE_ACCESS_WRITE:
      CHECK_ERR(resource.second.first->UseAsStorage(context, stage));
      break;
    default:
      break;
    }
  }

  return Error::Success();
}

inline auto IsHazard(const Ref<Graphics::Texture> &first,
                     const Ref<Graphics::Texture> &second) {
  if (first->imageMemory->image != second->imageMemory->image) {
    return false;
  }

  // Check if the two textures overlap in mip levels or array layers
  auto firstMipRange = std::make_pair(first->baseMipLevel,
                                      first->baseMipLevel + first->levelCount);
  auto secondMipRange = std::make_pair(
      second->baseMipLevel, second->baseMipLevel + second->levelCount);

  auto firstLayerRange = std::make_pair(
      first->baseArrayLayer, first->baseArrayLayer + first->layerCount);
  auto secondLayerRange = std::make_pair(
      second->baseArrayLayer, second->baseArrayLayer + second->layerCount);

  bool mipOverlap = firstMipRange.second > secondMipRange.first &&
                    secondMipRange.second > firstMipRange.first;
  bool layerOverlap = firstLayerRange.second > secondLayerRange.first &&
                      secondLayerRange.second > firstLayerRange.first;

  return mipOverlap && layerOverlap;
}

inline auto InsertTextureBarriers(const GraphicsContext &context) -> Error {
  auto shader = DynamicRendering::GetShader();

  for (auto &texturePair : shader->GetState().userBoundTextures) {
    auto &texture = texturePair.second;
    auto key = texturePair.first;

    texture.first->MarkUse();

    const auto *infoResult = shader->GetSlotDescription(key);
    if (infoResult == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound texture slot.");
    }

    const auto &info = infoResult->GetInfo<Reflect::SamplerInfo>();

    VkAccessFlags2 access = 0;

    switch (info.access) {
    case SLANG_RESOURCE_ACCESS_NONE:
    case SLANG_RESOURCE_ACCESS_READ:
      access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
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
    if (DynamicRendering::GetBindPoint() == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      const auto &targets = DynamicRendering::GetRenderTargets();
      for (const auto &target : targets) {
        if (IsHazard(texture.first, target.texture)) {
          auto debugname = texture.first->GetDebugName();
          return Error::Createf(
              "Texture {} is bound as a render target, but is also used as a "
              "shader "
              "resource. This is not supported and may indicate a bug in the "
              "application.",
              debugname);
        }
      }
    }
#endif

    Barrier::UpdateUsage(context, *texture.first,
                         {
                             .stages = stages,
                             .access = access,
                         });
  }

  return {};
}

inline auto InsertBufferBarriers(const GraphicsContext &context) -> Error {
  auto shader = DynamicRendering::GetShader();

  for (auto &bufferPair : shader->GetState().userBoundBuffers) {
    auto &buffer = bufferPair.second;
    auto key = bufferPair.first;

    buffer.first->MarkUse();

    const auto *slotInfo = shader->GetSlotDescription(key);
    if (slotInfo == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound buffer slot.");
    }
    if (!slotInfo->Is<Reflect::BufferInfo>()) {
      return Error::Create("Expected buffer info for bound buffer slot.");
    }

    const auto &info = slotInfo->GetInfo<Reflect::BufferInfo>();

    VkAccessFlags2 access = 0;

    switch (info.access) {
    case SLANG_RESOURCE_ACCESS_NONE:
      // Shaders with ubo buffers show access of NONE for some reason
      access = VK_ACCESS_2_UNIFORM_READ_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_READ:
      access = info.bufferType == Reflect::BufferType::Uniform
                   ? VK_ACCESS_2_UNIFORM_READ_BIT
                   : VK_ACCESS_2_SHADER_READ_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_READ_WRITE:
      access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    case SLANG_RESOURCE_ACCESS_WRITE:
      access = VK_ACCESS_2_SHADER_WRITE_BIT;
      break;
    default:
      PrintWarning("Buffer access type is Unknown for slang access: {}, "
                   "skipping barrier.",
                   static_cast<uint32_t>(info.access));
      break;
    }

    if (access == 0 && info.access != SLANG_RESOURCE_ACCESS_NONE) {
      PrintWarning("Buffer access type is Unknown for slang access: {}, "
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

    Barrier::UpdateUsage(context, *buffer.first,
                         {
                             .stages = stages,
                             .access = access,
                         });
  }

  return {};
}

inline auto InsertResourceBarriers(const GraphicsContext &context) -> Error {
  auto shader = DynamicRendering::GetShader();
  if (shader == nullptr) {
    return Error::Success(); // No shader, so nothing to update
  }

  CHECK_ERR(InsertTextureBarriers(context));
  CHECK_ERR(InsertBufferBarriers(context));

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
    DynamicRendering::CurrentStats.drawCalls++;
    DynamicRendering::CurrentStats.triangleCount +=
        static_cast<uint64_t>(mesh.GetIndexCount() * instanceCount);
    DynamicRendering::CurrentStats.instanceCount += instanceCount;

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
    ZoneScopedN("Resource management")

    {
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

auto Draw(const GraphicsContext &context, Texture &texture,
          uint32_t instanceCount) -> Error {
  ZoneScoped;

  if (!QuadMesh.isValid()) {
    QuadMesh = CHECK_RES(CreateQuad01Mesh(context));
  }

  auto shader = DynamicRendering::GetShader();

  if (shader == nullptr) {
    shader = DefaultShaderModule;
  }

  CHECK_ERR(shader->Send({"MainTexture"}, Ref<Texture>(&texture)));

  return Draw(context, *QuadMesh, instanceCount);
}

auto Dispatch(const GraphicsContext &context, const Math::Uvec3 &threadgroups)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  ERR_ASSERT(commandBuffer != nullptr);

  ERR_ASSERT(DynamicRendering::GetBindPoint() ==
             VK_PIPELINE_BIND_POINT_COMPUTE);
  CHECK_ERR(DynamicRendering::PrepareRendering(context));
  DynamicRendering::EndRendering(context);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  DynamicRendering::CurrentStats.dispatchCalls++;
  vkCmdDispatch(commandBuffer, threadgroups.x, threadgroups.y, threadgroups.z);

#if Enable_Snapshots
  CaptureEvent(DispatchEvent(threadgroups.x, threadgroups.y, threadgroups.z));
#endif

  return Error::Success();
}

auto DispatchWithin(const GraphicsContext &context, Math::Uvec3 dimensions)
    -> Error {
  ZoneScoped;

  const auto &shader = DynamicRendering::GetShader();
  if (shader == nullptr) {
    return Error::Create("No shader bound for dispatch call.");
  }

  const auto &threadgroupSize = CHECK_RES(shader->GetThreadgroupSize());

  // Allow passing in 0.
  dimensions.x = std::max(dimensions.x, 1U);
  dimensions.y = std::max(dimensions.y, 1U);
  dimensions.z = std::max(dimensions.z, 1U);

  Math::Uvec3 threadgroups{
      Utils::CeilDiv(dimensions.x, threadgroupSize.x),
      Utils::CeilDiv(dimensions.y, threadgroupSize.y),
      Utils::CeilDiv(dimensions.z, threadgroupSize.z),
  };

  return Dispatch(context, threadgroups);
}

auto DispatchIndirect(const GraphicsContext &context,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for dispatch indirect.");
  }

  ERR_ASSERT(DynamicRendering::GetBindPoint() ==
             VK_PIPELINE_BIND_POINT_COMPUTE);
  CHECK_ERR(DynamicRendering::PrepareRendering(context));
  DynamicRendering::EndRendering(context);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  DynamicRendering::CurrentStats.dispatchCalls++;
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
  ERR_ASSERT(DynamicRendering::GetBindPoint() ==
             VK_PIPELINE_BIND_POINT_GRAPHICS);
  auto &vertexFormat = mesh.GetVertexFormat();
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

  DynamicRendering::CurrentStats.drawCalls++;
  DynamicRendering::CurrentStats.triangleCount +=
      static_cast<uint64_t>(mesh.GetIndexCount() * count);
  DynamicRendering::CurrentStats.instanceCount += count;

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

  ERR_ASSERT(DynamicRendering::GetBindPoint() ==
             VK_PIPELINE_BIND_POINT_GRAPHICS);
  vkCmdSetVertexInputEXT(commandBuffer, 0, nullptr, 0, nullptr);
  GetThreadContext().currentVertexFormatHash = 0; // No vertex format
  DynamicRendering::SetTopology(topology);

  CHECK_ERR(UpdateShaderResourceLayouts(
      context, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
  CHECK_ERR(InsertResourceBarriers(context));

  CHECK_ERR(DynamicRendering::PrepareRendering(context));

  DynamicRendering::CurrentStats.drawCalls++;
  DynamicRendering::CurrentStats.triangleCount +=
      static_cast<uint64_t>(vertexCount * instanceCount);
  DynamicRendering::CurrentStats.instanceCount += instanceCount;

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

  ERR_ASSERT(DynamicRendering::GetBindPoint() ==
             VK_PIPELINE_BIND_POINT_GRAPHICS);
  vkCmdSetVertexInputEXT(commandBuffer, 0, nullptr, 0, nullptr);
  GetThreadContext().currentVertexFormatHash = 0; // No vertex format
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

  DynamicRendering::CurrentStats.drawCalls++;
  DynamicRendering::CurrentStats.triangleCount +=
      static_cast<uint64_t>(indexCount * instanceCount);
  DynamicRendering::CurrentStats.instanceCount += instanceCount;

  vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);

#if Enable_Snapshots
  CaptureEvent(DrawIndexedEvent(indexCount, instanceCount, 0, 0, 0));
#endif

  return Error::Success();
}

} // namespace Graphics