#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include <cassert>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Graphics {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<Ref<Texture::Texture>> ReleasedTextures;
extern std::vector<Ref<Buffer>> ReleasedBuffers;

extern std::mutex ReleasedTexturesMutex;
extern std::mutex ReleasedBuffersMutex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto CanBeDestroyed( // NOLINTNEXTLINE
    const std::unordered_map<QueueID, uint64_t> &completedTimelineValues,
    const std::unordered_map<QueueID, uint64_t> &resourceTimelineValues)
    -> bool {

  if (!GetDeferredDestructionAllowed()) {
    return true;
  }

  // NOLINTNEXTLINE
  for (const auto &[queueID, timelineValue] : resourceTimelineValues) {
    auto iterator = completedTimelineValues.find(queueID);
    if (iterator == completedTimelineValues.end() ||
        iterator->second <= timelineValue) {
      return false;
    }
  }
  return true;
}

inline auto ProcessReleasedResources(
    GraphicsContext &context,
    const std::unordered_map<QueueID, uint64_t> &completedTimelineValues)
    -> void {
  auto size = static_cast<int64_t>(ReleasedTextures.size());
  for (int64_t i = size - 1; i >= 0; i--) {
    auto &resource = ReleasedTextures.at(i);

    if (CanBeDestroyed(completedTimelineValues,
                       resource->GetTimelineValues())) {
      resource->Destroy(context);
      ReleasedTextures.erase(ReleasedTextures.begin() + i);
    }
  }

  size = static_cast<int64_t>(ReleasedBuffers.size());
  for (int64_t i = size - 1; i >= 0; i--) {
    auto &resource = ReleasedBuffers.at(i);

    if (CanBeDestroyed(completedTimelineValues,
                       resource->GetTimelineValues())) {
      resource->Destroy(context);
      ReleasedBuffers.erase(ReleasedBuffers.begin() + i);
    }
  }
}

auto ScheduleDestruction(Texture::Texture *texture) -> void;
auto ScheduleDestruction(Buffer *buffer) -> void;

} // namespace Graphics