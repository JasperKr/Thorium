#pragma once

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

struct MeshDrawRange {
  uint32_t Offset, Count;
};

static const Type meshType = Type("Mesh");

struct Mesh : Object {
  auto ScheduleDestroy() -> bool override;

  static auto Create(GraphicsContext &context, VertexFormat vertexFormat,
                     const std::span<uint8_t> &vertexData,
                     std::vector<uint32_t> *indexData)
      -> tl::expected<Ref<Mesh>, Error::Error>;

  auto Release() const -> void;

  [[nodiscard]] auto GetVertexFormat() const -> VertexFormat;

  [[nodiscard]] auto GetVertexCount() const -> uint32_t;
  [[nodiscard]] auto GetVertexData() const -> auto *;

  [[nodiscard]] auto GetIndexCount() const -> uint32_t;
  [[nodiscard]] auto GetIndexData() -> void *;

  void SetDrawRange(MeshDrawRange range);
  [[nodiscard]] auto GetDrawRange() const -> MeshDrawRange;

  auto SetTopology(VkPrimitiveTopology topology) -> Error::Error;
  [[nodiscard]] auto GetTopology() const -> VkPrimitiveTopology;

  auto Draw(GraphicsContext &context) const -> Error::Error;
  auto DrawInstanced(GraphicsContext &context, uint32_t instanceCount) const
      -> Error::Error;

  auto SetVertices(GraphicsContext &context,
                   const std::span<uint8_t> &vertexData, uint64_t offset = 0)
      -> Error::Error;
  auto SetIndices(GraphicsContext &context,
                  const std::span<uint32_t> &indexData, uint64_t offset = 0)
      -> Error::Error;

  auto SetVertexBuffer(const Ref<Buffer> &buffer) -> void;
  auto SetIndexBuffer(const Ref<Buffer> &buffer) -> void;

  static auto GetType() -> Type const * { return &meshType; }

private:
  auto UploadVertices(GraphicsContext &context,
                      const std::span<uint8_t> &vertices, uint64_t offset)
      -> Error::Error;
  auto UploadIndices(GraphicsContext &context,
                     const std::span<uint32_t> &indices, uint64_t offset)
      -> Error::Error;

  void Bind(VkCommandBuffer cmdBuffer) const;

  VertexFormat Format;

  Ref<Buffer> VertexBuffer;
  Ref<Buffer> IndexBuffer;

  MeshDrawRange DrawRange = {.Offset = 0, .Count = 0};
  uint64_t VertexCount;
  uint64_t IndexCount;

  VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
};

} // namespace Graphics