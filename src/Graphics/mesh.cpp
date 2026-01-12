#include "mesh.hpp"
#include <algorithm>
#include <sys/types.h>

#include "Graphics/barrier.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "buffer.hpp"
#include "graphics.hpp"
#include "tl/expected.hpp"
#include <cstdint>
#include <span>
#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

#include "vertexformat.hpp"

namespace Graphics {

auto Mesh::ScheduleDestroy() -> void {
  VertexBuffer->ScheduleDestroy();
  IndexBuffer->ScheduleDestroy();
}

static auto VertexFormatSize(VertexFormat &format, uint32_t binding)
    -> uint32_t {
  return format.GetBindings().at(binding).stride;
}

auto Mesh::UploadVertices(GraphicsContext &context,
                          const std::span<uint8_t> &vertices, uint64_t offset)
    -> Error {

  Barrier::InsertUsage(Barrier::ResourceState{
      .type = Barrier::UsageType::Write,
      .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
  });

  Barrier::FlushBarriers(context, Barrier::GlobalResourceUsageTimeline);

  return VertexBuffer->SetData(context, vertices, offset);
}

auto Mesh::UploadIndices(GraphicsContext &context,
                         const std::span<uint8_t> &indices, uint64_t offset,
                         IndexFormat format) -> Error {
  Barrier::InsertUsage(Barrier::ResourceState{
      .type = Barrier::UsageType::Write,
      .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
  });

  Barrier::FlushBarriers(context, Barrier::GlobalResourceUsageTimeline);

  return IndexBuffer->SetData(context, indices, offset);
}

auto Mesh::Create(GraphicsContext &context, VertexFormat vertexFormat,
                  const std::span<uint8_t> &vertexData) -> Result<Ref<Mesh>> {

  auto meshData = Ref<Mesh>::Make();
  auto *mesh = meshData.get();

  assert(vertexData.size() % VertexFormatSize(vertexFormat, 0) == 0);

  mesh->VertexCount = vertexData.size() / VertexFormatSize(vertexFormat, 0);

  mesh->IndexCount = 0;

  mesh->Format = vertexFormat;

  VkMemoryPropertyFlags properties =
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  Graphics::BufferCreationInfo vboCreationInfo = {};
  vboCreationInfo.usage =
      static_cast<uint32_t>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
      static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  vboCreationInfo.properties = properties;
  vboCreationInfo.size = vertexData.size();

  auto bufferResult = Buffer::Create(context, vboCreationInfo);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error().AsUnexpected();
  }

  mesh->VertexBuffer = bufferResult.value();

  mesh->DrawRange.Offset = 0;
  mesh->DrawRange.Count = mesh->VertexCount;

  Error error = mesh->UploadVertices(context, vertexData, 0);

  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  return meshData;
}

auto Mesh::Create(GraphicsContext &context, VertexFormat vertexFormat,
                  uint64_t vertexCount) // NOLINT
    -> Result<Ref<Mesh>> {
  auto size = VertexFormatSize(vertexFormat, 0);

  if (size == 0) {
    return Error::Create("Vertex format has zero size for binding 0.")
        .AsUnexpected();
  }

  auto vertexDataSize = vertexCount * size;

  PrintAlways("vertex data size: {}", vertexDataSize);

  std::vector<uint32_t> indexData;

  auto meshData = Ref<Mesh>::Make();
  auto *mesh = meshData.get();

  mesh->VertexCount = vertexCount;

  mesh->Format = vertexFormat;

  VkMemoryPropertyFlags properties =
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
      static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  Graphics::BufferCreationInfo vboCreationInfo = {};
  vboCreationInfo.usage =
      static_cast<uint32_t>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
      static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  vboCreationInfo.properties = properties;
  vboCreationInfo.size = vertexDataSize;

  PrintAlways("Creating VBO of size {}", vboCreationInfo.size);

  auto bufferResult = Buffer::Create(context, vboCreationInfo);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error().AsUnexpected();
  }

  mesh->VertexBuffer = bufferResult.value();

  mesh->DrawRange.Offset = 0;
  mesh->DrawRange.Count = mesh->VertexCount;

  return meshData;
}

auto Mesh::Release() const -> void {
  VertexBuffer->ScheduleDestroy();
  IndexBuffer->ScheduleDestroy();
}

[[nodiscard]] auto Mesh::GetVertexFormat() const -> VertexFormat {
  return Format;
}
[[nodiscard]] auto Mesh::GetVertexCount() const -> uint32_t {
  return VertexCount;
}
[[nodiscard]] auto Mesh::GetIndexCount() const -> uint32_t {
  return IndexCount;
}
void Mesh::SetDrawRange(MeshDrawRange range) {
  auto maxCount = IndexCount == 0 ? VertexCount : IndexCount;

  assert(range.Offset >= 0 && range.Offset + range.Count <= maxCount);

  DrawRange.Offset = range.Offset;
  DrawRange.Count = range.Count;
}
[[nodiscard]] auto Mesh::GetDrawRange() const -> MeshDrawRange {
  return DrawRange;
}

auto Mesh::SetVertices(GraphicsContext &context,
                       const std::span<uint8_t> &vertexData, uint64_t offset)
    -> Error {
  return UploadVertices(context, vertexData, offset);
}
auto Mesh::SetIndices(GraphicsContext &context,
                      const std::span<uint8_t> &indexData, IndexFormat format)
    -> Error {

  if (format == IndexFormat::None) {
    return Error::Create("Invalid Index Format: None");
  }

  auto newCount = indexData.size() / GetIndexFormatSize(format);

  if (IndexBuffer.get() == nullptr || IndexBuffer->size < indexData.size()) {
    if (IndexBuffer.get() != nullptr) {
      IndexBuffer->ScheduleDestroy();
    }

    Graphics::BufferCreationInfo iboCreationInfo = {};
    iboCreationInfo.usage =
        static_cast<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) |
        static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    VkMemoryPropertyFlags properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    iboCreationInfo.properties = properties;
    iboCreationInfo.size = indexData.size();

    auto bufferResult = Buffer::Create(context, iboCreationInfo);

    if (Error::IsError(bufferResult)) {
      return bufferResult.error();
    }

    IndexBuffer = bufferResult.value();
    IndexCount = newCount;

    DrawRange.Count = IndexCount;
    DrawRange.Offset =
        (std::min)(DrawRange.Offset, static_cast<uint32_t>(IndexCount - 1));
  }

  IndicesFormat = format;

  return UploadIndices(context, indexData, 0, format);
}

auto Mesh::SetVertexBuffer(const Ref<Buffer> &buffer) -> void {
  VertexBuffer = buffer;
}
auto Mesh::SetIndexBuffer(const Ref<Buffer> &buffer, IndexFormat format)
    -> Error {

  if (format == IndexFormat::None) {
    return Error::Create("Invalid Index Format: None");
  }

  IndexBuffer = buffer;
  IndicesFormat = format;

  return Error::Success();
}

auto Mesh::GetIndexFormat() const -> IndexFormat { return IndicesFormat; }

auto Mesh::GetVertexBuffer() const -> Ref<Buffer> { return VertexBuffer; }
auto Mesh::GetIndexBuffer() const -> Ref<Buffer> { return IndexBuffer; }

// Disallow: Fan, Geometry, Patch
constexpr std::array<VkPrimitiveTopology, 5> validTopologies = {
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,     VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

auto Mesh::SetTopology(VkPrimitiveTopology topology) -> Error {
  bool isValid = false;
  for (const auto &validTopology : validTopologies) {
    if (topology == validTopology) {
      isValid = true;
      break;
    }
  }

  if (!isValid) {
    return Error::Create("Invalid primitive topology for Mesh.");
  }

  Topology = topology;

  return Error::Success();
}
auto Mesh::GetTopology() const -> VkPrimitiveTopology { return Topology; }

} // namespace Graphics