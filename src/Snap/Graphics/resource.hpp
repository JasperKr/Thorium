#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include <cassert>
#include <mutex>

namespace Graphics {

struct GraphicsResource {
  GraphicsResource(const GraphicsResource &) = delete;
  GraphicsResource(GraphicsResource &&) = delete;
  auto operator=(const GraphicsResource &) -> GraphicsResource & = delete;
  auto operator=(GraphicsResource &&) -> GraphicsResource & = delete;

  // Callback for when the resource is no longer in use by the GPU
  virtual auto OnUnused() -> bool = 0;

  // Call when the resource is used
  auto Use() -> void {
    timelineValue = (std::max)(timelineValue, GetSemaphoreValue());
  }

  // Check if the resource is currently in use by the GPU
  [[nodiscard]] auto InUse() const -> bool {
    if (!GetDeferredDestructionAllowed()) {
      return false;
    }

    return IsInUse(timelineValue);
  }

  virtual ~GraphicsResource() = default;

  [[nodiscard]] auto GetTimestamp() const -> uint64_t { return timelineValue; }

private:
  uint64_t timelineValue{};
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<Ref<Texture::Texture>> ReleasedTextures;
extern std::vector<Ref<Buffer>> ReleasedBuffers;

extern std::mutex ReleasedTexturesMutex;
extern std::mutex ReleasedBuffersMutex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto ProcessReleasedResources(GraphicsContext &context) -> void;

auto ScheduleDestruction(Texture::Texture *texture) -> void;
auto ScheduleDestruction(Buffer *buffer) -> void;

} // namespace Graphics