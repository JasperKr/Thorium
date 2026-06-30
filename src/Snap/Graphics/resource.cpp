#include "resource.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include <mutex>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

// Destruction order is:
// Shaders: Can hold buffers and texture views, so they must be destroyed first.
// Texture views: Can hold textures, so they must be destroyed second.
// Textures
// Buffers

std::vector<TextureMemory> ReleasedTextures{};
std::vector<BufferMemory> ReleasedBuffers{};
std::vector<TextureViewMemory> ReleasedTextureViews{};
std::vector<ShaderModuleMemory> ReleasedShaderModules{};

std::mutex ReleasedTexturesMutex{};
std::mutex ReleasedBuffersMutex{};
std::mutex ReleasedTextureViewsMutex{};
std::mutex ReleasedShaderModulesMutex{};
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

auto ScheduleDestruction(const TextureViewMemory &textureView) -> void {
  std::lock_guard<std::mutex> lock(ReleasedTextureViewsMutex);

#ifndef NDEBUG
  // Debug check to ensure we are not double-releasing a texture view
  for (const auto &releasedTextureView : ReleasedTextureViews) {
    if (releasedTextureView.imageView == textureView.imageView) {
      PrintError("TextureView resource double release detected! Handle: {}",
                 (void *)textureView.imageView);
      assert(false && "TextureView resource double release detected!");
    }
  }
#endif
  ReleasedTextureViews.emplace_back(textureView);
}

auto ScheduleDestruction(const ShaderModuleMemory &shaderModule) -> void {
  std::lock_guard<std::mutex> lock(ReleasedShaderModulesMutex);

#ifndef NDEBUG
  // Debug check to ensure we are not double-releasing a shader module
  for (const auto &releasedShaderModule : ReleasedShaderModules) {
    if (releasedShaderModule.shaderModule == shaderModule.shaderModule) {
      PrintError("ShaderModule resource double release detected! Handle: {}",
                 (void *)shaderModule.shaderModule);
      assert(false && "ShaderModule resource double release detected!");
    }
  }
#endif
  ReleasedShaderModules.emplace_back(shaderModule);
}

auto TextureMemory::Destroy() -> void {
  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  auto *context = GetCurrentGraphicsContext();

  assert(context != nullptr && context->device != VK_NULL_HANDLE &&
         context->vmaAllocator != VK_NULL_HANDLE &&
         "Invalid graphics context during texture destruction.");

  vmaDestroyImage(context->vmaAllocator, image, allocation);

  image = VK_NULL_HANDLE;
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

auto TextureViewMemory::Destroy() -> void {
  std::unique_lock<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

  auto *context = GetCurrentGraphicsContext();

  vkDestroyImageView(context->device, imageView, GetAllocationCallbacks());

  imageView = VK_NULL_HANDLE;
}

auto ShaderModuleMemory::Destroy() -> void {
  std::unique_lock<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

  auto *context = GetCurrentGraphicsContext();

  vkDestroyShaderModule(context->device, shaderModule,
                        GetAllocationCallbacks());

  shaderModule = VK_NULL_HANDLE;
}

inline auto CanBeDestroyed( // NOLINTNEXTLINE
    const uint64_t &resourceTimelineValue) -> bool {

  if (!GetDeferredDestructionAllowed()) {
    return true;
  }

  return !Graphics::semaphoreManager.IsInUse(resourceTimelineValue);
}

auto ProcessReleasedResources(GraphicsContext &context) -> void {
  if (!GetDeferredDestructionAllowed()) {
    std::scoped_lock<std::mutex, std::mutex, std::mutex, std::mutex> lock(
        ReleasedTexturesMutex, ReleasedBuffersMutex, ReleasedTextureViewsMutex,
        ReleasedShaderModulesMutex);

    for (auto &res : ReleasedTextureViews) {
      res.Destroy();
    }
    ReleasedTextureViews.clear();

    for (auto &res : ReleasedTextures) {
      res.Destroy();
    }
    ReleasedTextures.clear();

    for (auto &res : ReleasedBuffers) {
      res.Destroy();
    }
    ReleasedBuffers.clear();

    for (auto &res : ReleasedShaderModules) {
      res.Destroy();
    }
    ReleasedShaderModules.clear();

    return;
  }

  {
    std::lock_guard<std::mutex> lock(ReleasedShaderModulesMutex);

    Utils::UnorderedErase(ReleasedShaderModules,
                          [&](ShaderModuleMemory &res) -> auto {
                            if (CanBeDestroyed(res.timelineValue)) {
                              res.Destroy();
                              return true;
                            }
                            return false;
                          });
  }

  {
    std::lock_guard<std::mutex> lock(ReleasedTextureViewsMutex);

    Utils::UnorderedErase(ReleasedTextureViews,
                          [&](TextureViewMemory &res) -> auto {
                            if (CanBeDestroyed(res.timelineValue)) {
                              res.Destroy();
                              return true;
                            }
                            return false;
                          });
  }

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