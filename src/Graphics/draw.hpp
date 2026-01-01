#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/rendertarget.hpp"
#include <cstdint>

namespace Graphics {
void BindMesh(VkCommandBuffer cmdBuffer, const Mesh &mesh);

auto Draw(GraphicsContext &context, const Mesh &mesh,
          uint32_t instanceCount = 1) -> Error;

auto Dispatch(GraphicsContext &context, const Shader::ShaderModule &shader,
              const Math::Uvec3 &threadgroups) -> Error;

auto DispatchIndirect(GraphicsContext &context,
                      const Shader::ShaderModule &shader,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error;

auto DrawIndirect(GraphicsContext &context, const Mesh &mesh,
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