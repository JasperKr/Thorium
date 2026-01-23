#include "resource.hpp"

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

} // namespace Graphics