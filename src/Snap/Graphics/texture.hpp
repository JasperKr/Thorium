#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/sampler.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Libraries/vma.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <atomic>
#include <mutex>
#include <span>

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace Graphics {

struct Buffer;

enum class TextureType : uint8_t {
  DEFAULT, // 2D texture, but we cannot start a variable with a number
  VOLUME,
  CUBEMAP,
  ARRAY,
};

enum class WrapMode : uint8_t {
  REPEAT,
  MIRRORED_REPEAT,
  CLAMP,
  CLAMPONE,
  CLAMPZERO
};

const static Type LuaTextureType = Type("Texture");

enum class TextureUsage : uint8_t {
  Sampler,
  Storage,
  Attachment,
  TransferSrc,
  TransferDst,
  PresentSrc,
  Unknown,
};

enum class TextureMipmapOption : uint8_t {
  None,   // Do not create mipmaps.
  Init,   // Calculate a mip chain.
  Manual, // Allocate a mip chain.
};

extern std::unordered_map<std::pair<VkFormat, TextureType>, Ref<struct Texture>,
                          struct VkFormatTextureTypeHash>
    DefaultTextureCache; // NOLINT

auto UnloadModule() -> void;

struct CopyRegion {
  VkOffset3D srcOffset{};
  VkOffset3D dstOffset{};
  VkExtent3D extent{};
  uint32_t srcBaseArrayLayer = 0;
  uint32_t dstBaseArrayLayer = 0;
  uint32_t layerCount = 1;
  uint32_t srcBaseMipLevel = 0;
  uint32_t dstBaseMipLevel = 0;
  uint32_t mipLevelCount = 1;
};

struct ToBufferCopyRegion {
  VkOffset3D srcOffset{};
  VkExtent3D extent{};
  uint32_t srcBaseArrayLayer = 0;
  uint32_t layerCount = 1;
  uint32_t srcBaseMipLevel = 0;
  uint32_t mipLevelCount = 1;
  VkDeviceSize dstOffset = 0;
};

struct Texture : Object, Barrier::BarrierSynced {
  std::mutex mutex;

  static std::atomic<VkDeviceSize> TotalAllocatedMemory;

  SamplerDescription samplerDescription{};
  VkExtent3D size{};

  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VmaAllocation memory = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  std::string debugName;

  uint64_t sizeInBytes = 0;
  uint64_t lastUsedTimestamp{};

  VkFormat format = VK_FORMAT_UNDEFINED;
  size_t mipmapcount{};
  size_t arrayLayers{1};
  VkImageUsageFlags usage{};

  VkPipelineStageFlagBits2 lastPipelineStage = VK_PIPELINE_STAGE_NONE_KHR;
  VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  TextureUsage lastUsage = TextureUsage::Unknown;

  // Flag to indicate if the sampler needs to be updated
  bool samplerDirty = false;

  // Safety flag to prevent double releases
  bool released = false;

  // Indicate if the texture has been destroyed, to avoid calling defer release multiple times
  bool isDestroyed = false;

  // Indicates if this texture is a swapchain view, which should not be destroyed
  bool isSwapchainView = false;

  auto UseAs(const GraphicsContext &context, TextureUsage newUsage,
             VkPipelineStageFlags2 stage) -> Error;

  auto UseAsAttachment(const GraphicsContext &context) -> Error;
  auto UseAsSampler(const GraphicsContext &context, VkPipelineStageFlags2 stage)
      -> Error;
  auto UseAsTransferSrc(const GraphicsContext &context) -> Error;
  auto UseAsTransferDst(const GraphicsContext &context) -> Error;
  auto UseAsStorage(const GraphicsContext &context, VkPipelineStageFlags2 stage)
      -> Error;
  auto UseAsPresentSrc(const GraphicsContext &context) -> Error;

  auto GetTimestamp() const -> uint64_t { return lastUsedTimestamp; }

  auto MarkUse() -> void {
    lastUsedTimestamp = (std::max)(lastUsedTimestamp, GetSemaphoreValue());
  }

  auto ScheduleDestroy() -> void override;
  auto UseDeferredDestruction() const -> bool override;

  auto CopyTo(const GraphicsContext &context, Texture &dstTexture,
              CopyRegion region) -> Error;

  auto CopyTo(const GraphicsContext &context, Buffer &dstBuffer,
              ToBufferCopyRegion region) -> Error;

  Texture() = default;
  Texture(const Texture &) = delete;
  auto operator=(const Texture &) -> Texture & = delete;
  Texture(Texture &&) noexcept = delete;
  auto operator=(Texture &&) noexcept -> Texture & = delete;

  [[nodiscard]] auto IsTexture() const -> bool override { return true; }
  [[nodiscard]] auto AsTexture() const -> struct Texture const * override {
    return this;
  }

  ~Texture() override;

  enum TextureType textureType = TextureType::DEFAULT;

  auto SetFilter(VkFilter minFilter, VkFilter magFilter,
                 VkSamplerMipmapMode mipFilter) -> void;
  [[nodiscard]] auto GetFilter() const
      -> std::tuple<VkFilter, VkFilter, VkSamplerMipmapMode>;
  auto SetBorderColor(VkBorderColor borderColor) -> void;
  [[nodiscard]] auto GetBorderColor() const -> VkBorderColor;
  auto SetAnisotropy(float anisotropy) -> void;
  [[nodiscard]] auto GetAnisotropy() const -> float;
  auto SetWrapmode(VkSamplerAddressMode addressModeU,
                   VkSamplerAddressMode addressModeV,
                   VkSamplerAddressMode addressModeW) -> void;
  [[nodiscard]] auto GetWrap() const
      -> std::tuple<VkSamplerAddressMode, VkSamplerAddressMode,
                    VkSamplerAddressMode>;
  auto SetLodBias(float mipLodBias) -> void;
  [[nodiscard]] auto GetLodBias() const -> float;
  auto SetLodRange(float minLod, float maxLod) -> void;
  [[nodiscard]] auto GetLodRange() const -> std::tuple<float, float>;
  auto SetDepthCompare(bool enable, VkCompareOp compareOp) -> void;
  [[nodiscard]] auto GetDepthCompare() const -> std::tuple<bool, VkCompareOp>;
  [[nodiscard]] auto GetWidth() const -> uint32_t { return size.width; };
  [[nodiscard]] auto GetHeight() const -> uint32_t { return size.height; };
  [[nodiscard]] auto GetDimensions() const -> VkExtent2D {
    return {size.width, size.height};
  };
  [[nodiscard]] auto GetDepth() const -> uint32_t { return size.depth; };
  auto GetSampler(const GraphicsContext &context) -> VkSampler;
  // Copies data from a 1D/2D/3D region from the provided data to the texture
  // sourceSize is the dimensions of the underlying data
  // sourceOffset is the offset within the data to start copying from
  // target is the offset within the texture to copy to
  // targetSize is the size of the region to copy within the texture
  auto SetPixels(const GraphicsContext &context,
                 const std::span<const uint8_t> &data,
                 uint32_t mipLevel, // NOLINT
                 uint32_t arrayLayer, VkExtent3D sourceSize,
                 VkOffset3D sourceOffset, // NOLINT
                 VkOffset3D target, VkExtent3D targetSize) -> Error;
  auto SetPixels(const GraphicsContext &context, Image::ImageData &imageData,
                 uint32_t mipLevel = 0, uint32_t arrayLayer = 0) -> Error;
  [[nodiscard]] auto GetMipmapCount() const -> size_t { return mipmapcount; }
  [[nodiscard]] auto GetFormat() const -> VkFormat { return format; }
  [[nodiscard]] auto SupportsStorage() const -> bool {
    return (usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
  }
  [[nodiscard]] auto SupportsSampling() const -> bool {
    return (usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;
  }
  [[nodiscard]] auto SupportsAttachment() const -> bool {
    return (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0 ||
           (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
  }

  [[nodiscard]] auto IsDepthTexture() const -> bool {
    return Image::IsDepthTexture(format);
  }

  [[nodiscard]] auto IsStencilTexture() const -> bool {
    return Image::IsStencilTexture(format);
  }

  static auto GetType() -> Type const * { return &LuaTextureType; }

  /// WARNING: Subresource ranges are not tracked. Make sure the range matches the global config of the texture after the transition.
  auto TransitionLayout(
      const GraphicsContext &context, VkImageLayout layout,
      VkPipelineStageFlags2 sourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
                                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VkPipelineStageFlags2 destinationStage =
          VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VkAccessFlags2 srcAccessMask = VK_ACCESS_NONE, // NOLINT
      VkAccessFlags2 dstAccessMask = VK_ACCESS_NONE,
      VkImageSubresourceRange range = {.levelCount = 1, .layerCount = 1})
      -> Error;

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return GetType();
  }

  [[nodiscard]] auto GetDebugName() const -> std::string_view {
    if (!debugName.empty()) {
      return debugName;
    }

    return "Unnamed Texture";
  }

  auto operator==(const Texture &other) const -> bool {
    return image == other.image && view == other.view &&
           memory == other.memory && format == other.format;
  }

  auto operator!=(const Texture &other) const -> bool {
    return !(*this == other);
  }
};

struct TextureCreationInfo {
  // Dimensions in texels
  VkExtent3D size{};

  // Amount of array layers
  uint32_t arrayLayers{1};

  // Texture format
  VkFormat format = VK_FORMAT_UNDEFINED;

  // Vulkan usage flags
  VkImageUsageFlags usage{};

  // Number of mipmap levels to allocate.
  int mipmapCount = 1;

  // Debug name
  std::string debugName;

  // Type of the texture (2D, Cubemap, etc.)
  TextureType textureType = TextureType::DEFAULT;
};

auto Create(const GraphicsContext &context, const TextureCreationInfo &info)
    -> Result<Ref<Texture>>;
auto FromSwapchainTexture(const GraphicsContext &context,
                          VkImage swapchainImage,
                          VkImageView swapchainImageView, VkFormat format,
                          uint32_t width, uint32_t height)
    -> Result<Ref<Texture>>;

auto CopyImageToBuffer(GraphicsContext &context, Texture *texture,
                       VkBuffer buffer) -> Error;
auto GenerateMipmaps(GraphicsContext &context, Texture *texture) -> Error;
auto LoadFromFile(GraphicsContext &context, const char *path,
                  VkImageUsageFlags usage = 0, TextureMipmapOption mipmaps = {})
    -> Result<Ref<Texture>>;

// texture 2D From byte array
auto LoadFromMemory(GraphicsContext &context,
                    const std::span<const uint8_t> &data,
                    VkImageUsageFlags usage = 0,
                    TextureMipmapOption mipmaps = {}) -> Result<Ref<Texture>>;

// texture 2D From ImageData
auto LoadFromMemory(GraphicsContext &context, Image::ImageData &imageData,
                    VkImageUsageFlags usage = 0,
                    TextureMipmapOption mipmaps = {}) -> Result<Ref<Texture>>;

// texture 3D/Array/Cubemap From array of ImageData slices
auto LoadFromMemory(GraphicsContext &context,
                    const std::vector<Image::ImageData *> &slices,
                    TextureType type, VkImageUsageFlags usage = 0,
                    TextureMipmapOption mipmaps = {}) -> Result<Ref<Texture>>;

auto GetDefaultTexture(const GraphicsContext &context, VkFormat format,
                       Graphics::TextureType textureType)
    -> Result<Ref<Graphics::Texture>>;

auto GetAccessFlagsForUsage(TextureUsage usage, VkFormat format)
    -> VkAccessFlags2;

} // namespace Graphics