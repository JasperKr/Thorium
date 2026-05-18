#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstddef>
#include <vulkan/vulkan_core.h>
namespace Graphics {

extern Ref<Buffer> BvhScratchBuffer;                                   // NOLINT
static constexpr size_t InitialScratchBufferSize = 16UL * 1024 * 1024; // 16 MiB

auto InitializeBVHModule(const struct GraphicsContext &context) -> Error;
auto DeInitializeBVHModule() -> void;

struct BVH : Object {
  // Create fast BLAS for the given mesh.
  static auto Create(const Mesh &mesh) -> Result<Ref<BVH>> {

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

    // Could be 16 or even 8 bit per component, depending on the model.
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = nullptr, // TODO: Fill this in.
    };

    return {};
  }
};

} // namespace Graphics