#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/Math/vector.hpp"
#include <cstdint>

namespace Graphics {
auto BindMesh(GraphicsContext &context, VkCommandBuffer cmdBuffer,
              const Mesh &mesh) -> Error;

auto Draw(GraphicsContext &context, Mesh &mesh, uint32_t instanceCount = 1)
    -> Error;

auto Dispatch(GraphicsContext &context, const Math::Uvec3 &threadgroups)
    -> Error;

auto DispatchIndirect(GraphicsContext &context,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error;

auto DrawIndirect(GraphicsContext &context, Mesh &mesh,
                  const Ref<Buffer> &indirectBuffer,
                  VkDeviceSize offset = 0, // NOLINT
                  uint32_t count = 1) -> Error;

// Vertex shader generated versions

auto Draw(GraphicsContext &context, const VkPrimitiveTopology &topology,
          uint32_t vertexCount, uint32_t instanceCount = 1) -> Error;

auto Draw(GraphicsContext &context, const Ref<Buffer> &indexBuffer,
          const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error;

} // namespace Graphics