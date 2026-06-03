#include "resource.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"

namespace Graphics {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<TextureMemory> ReleasedTextures{};
std::vector<BufferMemory> ReleasedBuffers{};

std::mutex ReleasedTexturesMutex{};
std::mutex ReleasedBuffersMutex{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto ScheduleDestruction(const TextureMemory &texture) -> void {
  std::lock_guard<std::mutex> lock(ReleasedTexturesMutex);

#ifndef NDEBUG
  // Debug check to ensure we are not double-releasing a texture
  for (const auto &releasedTexture : ReleasedTextures) {
    if (releasedTexture.image == texture.image) {
      PrintError("Texture resource double release detected! Handle: {}",
                 (void *)texture.image);
      assert(false && "Texture resource double release detected!");
    }
  }
#endif

  ReleasedTextures.emplace_back(texture);
}

auto ScheduleDestruction(const BufferMemory &buffer) -> void {
  std::lock_guard<std::mutex> lock(ReleasedBuffersMutex);

#ifndef NDEBUG
  // Debug check to ensure we are not double-releasing a buffer
  for (const auto &releasedBuffer : ReleasedBuffers) {
    if (releasedBuffer.buffer == buffer.buffer) {
      PrintError("Buffer resource double release detected! Handle: {}",
                 (void *)buffer.buffer);
      assert(false && "Buffer resource double release detected!");
    }
  }
#endif
  ReleasedBuffers.emplace_back(buffer);
}

auto TextureMemory::Destroy() -> void {
  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  auto *context = GetCurrentGraphicsContext();

  if (context == nullptr || context->device == VK_NULL_HANDLE ||
      context->vmaAllocator == VK_NULL_HANDLE) {
    return;
  }

  vkDestroyImageView(context->device, imageView, GetAllocationCallbacks());
  vmaDestroyImage(context->vmaAllocator, image, allocation);

  image = VK_NULL_HANDLE;
  imageView = VK_NULL_HANDLE;
  allocation = VK_NULL_HANDLE;
}

auto BufferMemory::Destroy() -> void {
  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  auto *context = GetCurrentGraphicsContext();

  vmaDestroyBuffer(context->vmaAllocator, buffer, allocation);

  buffer = VK_NULL_HANDLE;
  allocation = VK_NULL_HANDLE;
}

inline auto CanBeDestroyed( // NOLINTNEXTLINE
    const uint64_t &resourceTimelineValue) -> bool {

  if (!GetDeferredDestructionAllowed()) {
    return true;
  }

  return !IsInUse(resourceTimelineValue);
}

auto ProcessReleasedResources(GraphicsContext &context) -> void {
  // TODO: Issue with buffers
  // PrintAlways("Processing {} released textures and {} released buffers.",
  //             ReleasedTextures.size(), ReleasedBuffers.size());

  {
    std::lock_guard<std::mutex> lock(ReleasedTexturesMutex);

    Utils::UnorderedErase(ReleasedTextures, [&](TextureMemory &res) -> auto {
      if (CanBeDestroyed(res.timelineValue)) {
        res.Destroy();

        return true;
      }
      return false;
    });
  }

  {
    std::lock_guard<std::mutex> lock(ReleasedBuffersMutex);

    Utils::UnorderedErase(ReleasedBuffers, [&](BufferMemory &res) -> auto {
      if (CanBeDestroyed(res.timelineValue)) {
        res.Destroy();
        return true;
      }
      return false;
    });
  }
}

} // namespace Graphics