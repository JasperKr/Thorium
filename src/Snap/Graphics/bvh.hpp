#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstddef>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {

extern Ref<Buffer> BvhScratchBuffer;                                   // NOLINT
static constexpr size_t InitialScratchBufferSize = 16UL * 1024 * 1024; // 16 MiB

auto InitializeBVHModule(const struct GraphicsContext &context) -> Error;
auto DeInitializeBVHModule() -> void;

static const Type LuaBLASType = Type("BottomLevelAccelerationStructure");
static const Type LuaTLASType = Type("TopLevelAccelerationStructure");

// Bottom-Level Acceleration Structure.
struct BLAS : Object {
  static auto Create(const GraphicsContext &context, const Mesh &mesh)
      -> Result<Ref<BLAS>>;

  static auto GetType() -> Type const * { return &LuaBLASType; }
  auto GetInstanceType() const -> Type const * override {
    return BLAS::GetType();
  }

  [[nodiscard]] auto GetDeviceAddress() const -> VkDeviceAddress;

private:
  Ref<Buffer> accelerationStructureBuffer;
  VkDeviceAddress accelerationStructureAddress = 0;
  VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
};

// Top-Level Acceleration Structure.
struct TLAS : Object {
  static auto Create(const GraphicsContext &context) -> Result<Ref<TLAS>>;

  static auto GetType() -> Type const * { return &LuaTLASType; }
  auto GetInstanceType() const -> Type const * override {
    return TLAS::GetType();
  }

  [[nodiscard]] auto GetDeviceAddress() const -> VkDeviceAddress;

  auto AddInstance(const Ref<BLAS> &blas, const Math::Matrix4x4 &transform)
      -> uint32_t;
  auto RemoveInstance(uint32_t index) -> void;
  auto UpdateInstance(uint32_t index, const Math::Matrix4x4 &transform) -> void;

  auto GetInstances() const
      -> const std::vector<VkAccelerationStructureInstanceKHR> & {
    return instances;
  }

  auto GetInstanceBuffer() const -> Ref<Buffer> { return instanceBuffer; }

  auto GetAccelerationStructure() const -> VkAccelerationStructureKHR {
    return accelerationStructure;
  }

  auto Refit(const GraphicsContext &context) -> Error;
  auto Rebuild(const GraphicsContext &context) -> Error;

private:
  Ref<Buffer> accelerationStructureBuffer;
  VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
  VkDeviceAddress accelerationStructureAddress = 0;

  std::vector<VkAccelerationStructureInstanceKHR> instances;

  Ref<Buffer> instanceBuffer;
  uint32_t instanceCount = 0;
};

} // namespace Graphics