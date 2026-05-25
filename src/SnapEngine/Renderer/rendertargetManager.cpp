#include "rendertargetManager.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>

namespace Engine::Renderer {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
RenderTargetManager GlobalRenderTargetManager;

auto RendertargetDescriptor::Score(const RendertargetDescriptor &other) const
    -> int {
  uint32_t score = 0;
  constexpr auto SizeMatchScore = 10;
  constexpr auto SizeDiffPixelCount = 100;
  constexpr auto MipmapMatchScore = 10;
  constexpr auto UsageMatchScore = 5;

  if ((usage & other.usage) == 0) {
    // If the usage does not match, this rendertarget cannot be used.
    return -1;
  }

  if (usage == other.usage) {
    score += UsageMatchScore;
  }

  if (size.x < other.size.x || size.y < other.size.y) {
    // This rendertarget is too small for the requirements.
    return -1;
  }
  if (size.x == other.size.x && size.y == other.size.y) {
    score += SizeMatchScore;
  } else {
    // The rendertarget is larger than the requirements, deduct points based on difference.
    auto diff = std::max(size.x - other.size.x, size.y - other.size.y);
    score += std::max(1U, SizeMatchScore - (diff / SizeDiffPixelCount));
  }

  if (mipmapCount < other.mipmapCount) {
    // This rendertarget doesn't have enough mip levels for the requirements.
    return -1;
  }
  if (mipmapCount >= other.mipmapCount) {
    auto distance = static_cast<int>(mipmapCount - other.mipmapCount);
    score += std::max(1, MipmapMatchScore - distance);
  }

  if (format != other.format) {
    return -1;
  }

  return static_cast<int>(score);
};

auto RendertargetDescriptor::operator==(
    const RendertargetDescriptor &other) const -> bool {
  return size == other.size && mipmapCount == other.mipmapCount &&
         format == other.format && minFilter == other.minFilter &&
         magFilter == other.magFilter && mipFilter == other.mipFilter &&
         addressModeU == other.addressModeU &&
         addressModeV == other.addressModeV &&
         addressModeW == other.addressModeW &&
         borderColor == other.borderColor && usage == other.usage;
}

auto RendertargetDescriptor::operator!=(
    const RendertargetDescriptor &other) const -> bool {
  return !(*this == other);
}

auto RendertargetDescriptorHash::operator()(
    const RendertargetDescriptor &desc) const -> size_t {
  Hash::Hasher hasher;
  hasher.Add(desc.size.x);
  hasher.Add(desc.size.y);
  hasher.Add(desc.mipmapCount);
  hasher.Add(desc.format);
  hasher.Add(desc.minFilter);
  hasher.Add(desc.magFilter);
  hasher.Add(desc.mipFilter);
  hasher.Add(desc.addressModeU);
  hasher.Add(desc.addressModeV);
  hasher.Add(desc.addressModeW);
  hasher.Add(desc.borderColor);
  hasher.Add(desc.usage);
  return hasher.Get();
}

auto RenderTargetManager::GetRendertarget(
    const struct ::Graphics::GraphicsContext &context,
    const RendertargetDescriptor &descriptor)
    -> Result<Ref<::Graphics::Texture>> {
  // First try to find an exact match.
  for (auto &entry : Rendertargets) {
    if (entry.descriptor == descriptor && !entry.inUse) {
      entry.inUse = true;
      return entry.texture;
    }
  }

  // If no exact match, find the best candidate that meets the requirements.
  int bestScore = -1;
  int bestTextureIndex = -1;
  Ref<::Graphics::Texture> bestTexture;
  for (auto i = 0; i < Rendertargets.size(); i++) {
    const auto &entry = Rendertargets[i];

    int score = entry.descriptor.Score(descriptor);
    if (score > bestScore && !entry.inUse) {
      bestScore = score;
      bestTexture = entry.texture;
      bestTextureIndex = i;
    }
  }

  if (bestTextureIndex != -1) {
    ReconfigureTexture(descriptor, bestTexture);
    auto &entry = Rendertargets[bestTextureIndex];
    assert(!entry.inUse);

    entry.descriptor = descriptor;
    entry.inUse = true;

    return bestTexture;
  }

  if (Rendertargets.size() >= MaxRendertargets) {
    return Error::Unexpectedf("Exceeded maximum number of rendertargets ({}).",
                              MaxRendertargets);
  }

  auto info = ::Graphics::TextureCreationInfo{
      .size = {descriptor.size.x, descriptor.size.y, 1},
      .format = descriptor.format,
      .usage = descriptor.usage,
      .mipmapCount = static_cast<int>(descriptor.mipmapCount),
      .debugName =
          std::format("Rendertarget / {} / {}x{} #{}",
                      Graphics::Format::ImageFormatToString(descriptor.format),
                      std::to_string(descriptor.size.x),
                      std::to_string(descriptor.size.y), Rendertargets.size()),
  };

  auto texture = CHECK_RES(::Graphics::Create(context, info));

  Rendertargets.push_back(
      {.descriptor = descriptor, .inUse = true, .texture = texture});
  return texture;
}

auto RenderTargetManager::ReleaseRendertarget(
    const Ref<::Graphics::Texture> &texture) -> void {
  if (texture == nullptr) {
    return;
  }
  bool found = false;

  for (auto &entry : Rendertargets) {
    if (entry.texture == texture) {
      assert(!found && "Texture released multiple times.");
      entry.inUse = false;
      found = true;
    }
  }
}

auto RenderTargetManager::ReconfigureTexture(
    const RendertargetDescriptor &descriptor,
    const Ref<::Graphics::Texture> &texture) -> void {
  texture->SetFilter(descriptor.minFilter, descriptor.magFilter,
                     descriptor.mipFilter);
  texture->SetWrapmode(descriptor.addressModeU, descriptor.addressModeV,
                       descriptor.addressModeW);
  texture->SetBorderColor(descriptor.borderColor);
}

} // namespace Engine::Renderer