#include "resource.hpp"
#include "Graphics/buffer.hpp"
#include "Modules/console.hpp"
#include "Modules/utils.hpp"

namespace Graphics {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<Ref<Texture::Texture>> ReleasedTextures{};
std::vector<Ref<Buffer>> ReleasedBuffers{};

std::mutex ReleasedTexturesMutex{};
std::mutex ReleasedBuffersMutex{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto ScheduleDestruction(Texture::Texture *texture) -> void {
  std::lock_guard<std::mutex> lock(ReleasedTexturesMutex);
  ReleasedTextures.emplace_back(texture);
}

auto ScheduleDestruction(Buffer *buffer) -> void {
  std::lock_guard<std::mutex> lock(ReleasedBuffersMutex);
  ReleasedBuffers.emplace_back(buffer);
}

inline auto CanBeDestroyed( // NOLINTNEXTLINE
    const uint64_t &completedTimelineValue,
    const uint64_t &resourceTimelineValue) -> bool {

  if (!GetDeferredDestructionAllowed()) {
    return true;
  }

  return completedTimelineValue > resourceTimelineValue;
}

auto ProcessReleasedResources(GraphicsContext &context,
                              uint64_t completedTimelineValue) -> void {

  {
    std::lock_guard<std::mutex> lock(ReleasedTexturesMutex);

    auto size = static_cast<int64_t>(ReleasedTextures.size());
    for (int64_t i = size - 1; i >= 0; i--) {
      auto &resource = ReleasedTextures.at(i);

      if (CanBeDestroyed(completedTimelineValue, resource->GetTimestamp())) {
        Utils::UnorderedErase(
            ReleasedTextures,
            [&resource](const Ref<Graphics::Texture::Texture> &res) -> auto {
              return resource.get() == res.get();
            });
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(ReleasedBuffersMutex);

    auto size = static_cast<int64_t>(ReleasedBuffers.size());
    for (int64_t i = size - 1; i >= 0; i--) {
      const auto &resource = ReleasedBuffers.at(i);

      if (CanBeDestroyed(completedTimelineValue, resource->GetTimestamp())) {
        PrintAlways("Destroying buffer resource with timestamp {}; Handle: {}",
                    resource->GetTimestamp(), (void *)resource->handle);
        Utils::UnorderedErase(
            ReleasedBuffers,
            [&resource](const Ref<Graphics::Buffer> &res) -> auto {
              return resource.get() == res.get();
            });
      }
    }
  }
}

} // namespace Graphics