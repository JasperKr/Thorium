#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "buffer.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "vulkan/vulkan_core.h"

#include "vertexformat.hpp"

namespace Graphics {

struct MeshDrawRange {
  uint32_t Offset, Count;
};

inline auto GetIndexFormatSize(VkIndexType format) -> size_t {
  switch (format) {
  case VK_INDEX_TYPE_UINT8:
    return 1;
  case VK_INDEX_TYPE_UINT16:
    return 2;
  case VK_INDEX_TYPE_UINT32:
    return 4;
  default:
    return 0;
  }
}

static const Type meshType = Type("Mesh");

struct Mesh : Object {

  // Vertex data must be laid out as tightly packed arrays;
  // for example, 2 triangles with 2 bindings: [0, 1, 2, 0, 1, 2], [0, 1, 2, 0, 1, 2]
  // where the first array is for binding 0 and the second array is for binding 1.
  static auto Create(const GraphicsContext &context,
                     const VertexFormat &vertexFormat,
                     const std::vector<std::span<uint8_t>> &vertexData,
                     const std::string &debugName = "Mesh")
      -> Result<Ref<Mesh>>;

  static auto Create(const GraphicsContext &context,
                     const VertexFormat &vertexFormat, uint64_t vertexCount,
                     const std::string &debugName = "Mesh")
      -> Result<Ref<Mesh>>;

  [[nodiscard]] auto GetVertexFormat() -> VertexFormat &;

  [[nodiscard]] auto GetVertexCount() const -> uint32_t;
  [[nodiscard]] auto GetVertexData() const -> auto *;

  [[nodiscard]] auto GetIndexCount() const -> uint32_t;
  [[nodiscard]] auto GetIndexData() -> void *;
  [[nodiscard]] auto GetIndexFormat() const -> VkIndexType;

  void SetDrawRange(MeshDrawRange range);
  [[nodiscard]] auto GetDrawRange() const -> MeshDrawRange;

  auto SetTopology(VkPrimitiveTopology topology) -> Error;
  [[nodiscard]] auto GetTopology() const -> VkPrimitiveTopology;

  auto SetVertices(const GraphicsContext &context, uint32_t binding,
                   const std::span<const uint8_t> &vertexData,
                   uint64_t offset = 0) -> Error;
  auto SetIndices(const GraphicsContext &context,
                  const std::span<uint8_t> &indexData, VkIndexType format)
      -> Error;

  auto SetVertexBuffer(const Ref<Buffer> &buffer, uint32_t binding = 0) -> void;
  auto SetIndexBuffer(const Ref<Buffer> &buffer, VkIndexType format) -> Error;

  [[nodiscard]] auto GetVertexBuffer(uint32_t binding = 0) const -> Ref<Buffer>;
  [[nodiscard]] auto GetIndexBuffer() const -> Ref<Buffer>;

  static auto GetType() -> Type const * { return &meshType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return Mesh::GetType();
  }

  auto GetHash() const -> size_t {
    Hash::Hasher hasher;
    hasher.Add(VertexCount);
    hasher.Add(IndexCount);
    hasher.Add(IndicesFormat);
    hasher.Add(Topology);
    for (const auto &vbo : VertexBuffers) {
      hasher.Add(vbo ? vbo->handle : nullptr);
    }
    hasher.Add(IndexBuffer ? IndexBuffer->handle : nullptr);
    return hasher.Get();
  }

  auto GetVertexBuffers() const -> const std::vector<Ref<Buffer>> & {
    return VertexBuffers;
  }

  auto GetBindingCount() const -> uint32_t {
    return static_cast<uint32_t>(VertexBuffers.size());
  }

  struct VertexBindingRange {
    uint32_t firstBinding;
    uint32_t bindingCount;
    VkBuffer *bindings;
    VkDeviceSize *offsets;
  };

  auto GetBindingRanges() const -> std::span<const VertexBindingRange> {
    return {BindingRanges.data(), BindingRangeCount};
  }

private:
  auto UploadVertices(const GraphicsContext &context, uint32_t binding,
                      const std::span<const uint8_t> &vertices, uint64_t offset)
      -> Error;
  auto UploadIndices(const GraphicsContext &context,
                     const std::span<uint8_t> &indices, uint64_t offset,
                     VkIndexType format) -> Error;
  auto ConstructBindingRanges() -> void;

  std::array<VkBuffer, VertexFormat::MaxBindings> Bindings;
  std::array<VkDeviceSize, VertexFormat::MaxBindings> BindingOffsets;

  std::array<VertexBindingRange, VertexFormat::MaxBindings> BindingRanges;
  size_t BindingRangeCount = 0;

  VertexFormat Format;

  std::vector<Ref<Buffer>> VertexBuffers;
  Ref<Buffer> IndexBuffer;

  MeshDrawRange DrawRange = {.Offset = 0, .Count = 0};
  uint64_t VertexCount;
  uint64_t IndexCount;
  VkIndexType IndicesFormat;
  std::string DebugName;

  VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
};

} // namespace Graphics