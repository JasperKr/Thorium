#pragma once

#include "Graphics/graphics.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Graphics {

struct GraphicsMemory {
  uint64_t timelineValue{};

  void (*destroy)(const GraphicsContext &context,
                  void *resourceHandle) = nullptr;

  // Generic handle to the resource-specific data
  // NOLINTNEXTLINE
  std::unique_ptr<std::byte[]> resourceHandle;
};

struct BufferMemory {
  VmaAllocation allocation{};
  VkBuffer buffer{};

  static auto Destroy(const GraphicsContext &context, void *data) -> void;
};

struct TextureMemory {
  VmaAllocation allocation;
  VkImage image;

  static auto Destroy(const GraphicsContext &context, void *data) -> void;
};

struct TextureViewMemory {
  VkImageView imageView;

  static auto Destroy(const GraphicsContext &context, void *data) -> void;
};

struct ShaderModuleMemory {
  VkShaderModule shaderModule;

  static auto Destroy(const GraphicsContext &context, void *data) -> void;
};

struct PipelineMemory {
  VkPipeline pipeline;
  // VkPipelineLayout layout;
  // std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

  static auto Destroy(const GraphicsContext &context, void *data) -> void;
};

struct AccelerationStructureMemory {
  VkAccelerationStructureKHR accelerationStructure;

  static auto Destroy(const GraphicsContext &context, void *data) -> void;
};

struct GraphicsResource {
  GraphicsResource(const GraphicsResource &) = delete;
  GraphicsResource(GraphicsResource &&) = delete;
  auto operator=(const GraphicsResource &) -> GraphicsResource & = delete;
  auto operator=(GraphicsResource &&) -> GraphicsResource & = delete;

  // Call when the resource is used
  auto Use() -> void {
    timelineValue = Graphics::SemaphoreManager::GetSemaphoreValue();
  }

  // Check if the resource is currently in use by the GPU
  [[nodiscard]] auto InUse() const -> bool {
    if (!GetDeferredDestructionAllowed()) {
      return false;
    }

    return Graphics::semaphoreManager.IsInUse(timelineValue);
  }

  virtual ~GraphicsResource() = default;

  [[nodiscard]] auto GetTimestamp() const -> uint64_t { return timelineValue; }

private:
  uint64_t timelineValue{};
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<GraphicsMemory> ReleasedGraphicsMemory;
extern std::mutex ReleasedGraphicsMemoryMutex;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto ProcessReleasedResources(GraphicsContext &context) -> void;

auto ScheduleDestruction(const TextureMemory &texture, uint64_t timelineValue)
    -> void;
auto ScheduleDestruction(const BufferMemory &buffer, uint64_t timelineValue)
    -> void;
auto ScheduleDestruction(const TextureViewMemory &textureView,
                         uint64_t timelineValue) -> void;
auto ScheduleDestruction(const ShaderModuleMemory &shaderModule,
                         uint64_t timelineValue) -> void;
auto ScheduleDestruction(const PipelineMemory &pipeline, uint64_t timelineValue)
    -> void;
auto ScheduleDestruction(const AccelerationStructureMemory &resource,
                         uint64_t timelineValue) -> void;

} // namespace Graphics