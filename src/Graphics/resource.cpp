#include "resource.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Graphics/texture.hpp"
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

#ifndef NDEBUG
  // Debug check to ensure we are not double-releasing a texture
  for (const auto &releasedTexture : ReleasedTextures) {
    if (releasedTexture.get() == texture) {
      PrintError("Texture resource double release detected! Handle: {}",
                 (void *)texture->image);
      assert(false && "Texture resource double release detected!");
    }
  }
#endif

  ReleasedTextures.emplace_back(texture);
}

auto ScheduleDestruction(Buffer *buffer) -> void {
  std::lock_guard<std::mutex> lock(ReleasedBuffersMutex);

#ifndef NDEBUG
  // Debug check to ensure we are not double-releasing a buffer
  for (const auto &releasedBuffer : ReleasedBuffers) {
    if (releasedBuffer.get() == buffer) {
      PrintError("Buffer resource double release detected! Handle: {}",
                 (void *)buffer->handle);
      assert(false && "Buffer resource double release detected!");
    }
  }
#endif
  ReleasedBuffers.emplace_back(buffer);
}

inline auto CanBeDestroyed( // NOLINTNEXTLINE
    const uint64_t &resourceTimelineValue) -> bool {

  if (!GetDeferredDestructionAllowed()) {
    return true;
  }

  return !IsInUse(resourceTimelineValue);
}

auto ProcessReleasedResources(GraphicsContext &context) -> void {

  {
    std::lock_guard<std::mutex> lock(ReleasedTexturesMutex);

    Utils::UnorderedErase(
        ReleasedTextures,
        [&](const Ref<Graphics::Texture::Texture> &res) -> auto {
          if (CanBeDestroyed(res->GetTimestamp())) {
            res->isDestroyed = true;
            return true;
          }
          return false;
        });
  }

  {
    std::lock_guard<std::mutex> lock(ReleasedBuffersMutex);

    Utils::UnorderedErase(ReleasedBuffers,
                          [&](const Ref<Graphics::Buffer> &res) -> auto {
                            if (CanBeDestroyed(res->GetTimestamp())) {
                              res->isDestroyed = true;
                              return true;
                            }
                            return false;
                          });
  }
}

} // namespace Graphics