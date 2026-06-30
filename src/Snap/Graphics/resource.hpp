#pragma once

#include "Graphics/graphics.hpp"
#include <cassert>
#include <mutex>

namespace Graphics {

struct BufferMemory {
  VmaAllocation allocation;
  VkBuffer buffer;

  uint64_t timelineValue;

  auto Destroy() -> void;
};

struct TextureMemory {
  VmaAllocation allocation;
  VkImage image;

  uint64_t timelineValue;

  auto Destroy() -> void;
};

struct TextureViewMemory {
  VkImageView imageView;
  uint64_t timelineValue;

  auto Destroy() -> void;
};

struct ShaderModuleMemory {
  VkShaderModule shaderModule;
  uint64_t timelineValue;

  auto Destroy() -> void;
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
extern std::vector<TextureMemory> ReleasedTextures;
extern std::vector<BufferMemory> ReleasedBuffers;
extern std::vector<TextureViewMemory> ReleasedTextureViews;
extern std::vector<ShaderModuleMemory> ReleasedShaderModules;

extern std::mutex ReleasedTexturesMutex;
extern std::mutex ReleasedBuffersMutex;
extern std::mutex ReleasedTextureViewsMutex;
extern std::mutex ReleasedShaderModulesMutex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto ProcessReleasedResources(GraphicsContext &context) -> void;

auto ScheduleDestruction(const TextureMemory &texture) -> void;
auto ScheduleDestruction(const BufferMemory &buffer) -> void;
auto ScheduleDestruction(const TextureViewMemory &textureView) -> void;
auto ScheduleDestruction(const ShaderModuleMemory &shaderModule) -> void;

} // namespace Graphics