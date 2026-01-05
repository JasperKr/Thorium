#include "mesh.hpp"
#include <sys/types.h>

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
  return VertexBuffer->SetData(context, vertices, offset);
}

auto Mesh::UploadIndices(GraphicsContext &context,
                         const std::span<uint32_t> &indices, uint64_t offset)
    -> Error {

  auto uint8Span = // NOLINTNEXTLINE
      std::span<uint8_t>(reinterpret_cast<uint8_t *>(indices.data()),
                         indices.size() * sizeof(uint32_t));

  return IndexBuffer->SetData(context, uint8Span, offset);
}

auto Mesh::Create(GraphicsContext &context, VertexFormat vertexFormat,
                  const std::span<uint8_t> &vertexData,
                  std::vector<uint32_t> *indexData) -> Result<Ref<Mesh>> {

  auto meshData = Ref<Mesh>::Make();
  auto *mesh = meshData.get();

  assert(vertexData.size() % VertexFormatSize(vertexFormat, 0) == 0);

  mesh->VertexCount = vertexData.size() / VertexFormatSize(vertexFormat, 0);

  bool hasIndices = indexData != nullptr;
  mesh->IndexCount = hasIndices ? static_cast<uint32_t>(indexData->size()) : 0;

  uint64_t indicesSize = mesh->IndexCount * sizeof(uint32_t);

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

  Graphics::BufferCreationInfo iboCreationInfo = {};
  iboCreationInfo.usage =
      static_cast<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) |
      static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  iboCreationInfo.properties = properties;
  iboCreationInfo.size = indicesSize;

  bufferResult = Buffer::Create(context, iboCreationInfo);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error().AsUnexpected();
  }

  mesh->IndexBuffer = bufferResult.value();

  mesh->DrawRange.Offset = 0;
  mesh->DrawRange.Count =
      mesh->IndexCount > 0 ? mesh->IndexCount : vertexData.size();

  Error error = mesh->UploadVertices(context, vertexData, 0);

  if (Error::IsError(error)) {
    return error.AsUnexpected();
  }

  if (hasIndices) {
    auto indexSpan = std::span<uint32_t>(indexData->data(), indexData->size());

    error = mesh->UploadIndices(context, indexSpan, 0);

    if (Error::IsError(error)) {
      return error.AsUnexpected();
    }
  }

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
  assert(range.Offset >= 0 && range.Offset + range.Count <= IndexCount);

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
                      const std::span<uint32_t> &indexData, uint64_t offset)
    -> Error {
  return UploadIndices(context, indexData, offset);
}

auto Mesh::SetVertexBuffer(const Ref<Buffer> &buffer) -> void {
  VertexBuffer = buffer;
}
auto Mesh::SetIndexBuffer(const Ref<Buffer> &buffer) -> void {
  IndexBuffer = buffer;
}
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

  auto count = IndexCount > 0 ? IndexCount : VertexCount;

  switch (topology) {
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

  Topology = topology;

  return Error::Success();
}
auto Mesh::GetTopology() const -> VkPrimitiveTopology { return Topology; }

} // namespace Graphics