#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "buffer.hpp"
#include "graphics.hpp"
#include <cstdint>
#include <span>
#include <string>

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
  auto ScheduleDestroy() -> void override;
  auto UseDeferredDestruction() const -> bool override {
    return Graphics::GetDeferredDestructionAllowed();
  }

  static auto Create(GraphicsContext &context, VertexFormat vertexFormat,
                     const std::span<uint8_t> &vertexData,
                     const std::string &debugName = "Mesh")
      -> Result<Ref<Mesh>>;

  static auto Create(GraphicsContext &context, VertexFormat vertexFormat,
                     uint64_t vertexCount,
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

  auto SetVertices(GraphicsContext &context,
                   const std::span<const uint8_t> &vertexData,
                   uint64_t offset = 0) -> Error;
  auto SetIndices(GraphicsContext &context, const std::span<uint8_t> &indexData,
                  VkIndexType format) -> Error;

  auto SetVertexBuffer(const Ref<Buffer> &buffer) -> void;
  auto SetIndexBuffer(const Ref<Buffer> &buffer, VkIndexType format) -> Error;

  [[nodiscard]] auto GetVertexBuffer() const -> Ref<Buffer>;
  [[nodiscard]] auto GetIndexBuffer() const -> Ref<Buffer>;

  static auto GetType() -> Type const * { return &meshType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return Mesh::GetType();
  }

private:
  auto UploadVertices(GraphicsContext &context,
                      const std::span<const uint8_t> &vertices, uint64_t offset)
      -> Error;
  auto UploadIndices(GraphicsContext &context,
                     const std::span<uint8_t> &indices, uint64_t offset,
                     VkIndexType format) -> Error;

  VertexFormat Format;

  Ref<Buffer> VertexBuffer;
  Ref<Buffer> IndexBuffer;

  MeshDrawRange DrawRange = {.Offset = 0, .Count = 0};
  uint64_t VertexCount;
  uint64_t IndexCount;
  VkIndexType IndicesFormat;
  std::string DebugName;

  VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
};

} // namespace Graphics