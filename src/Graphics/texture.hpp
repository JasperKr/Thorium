#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/sampler.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphics.hpp"
#include <mutex>
#include <span>

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cstdint>

namespace Graphics::Texture {

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

const static Type type = Type("Texture");

enum class TextureUsage : uint8_t {
  Sampler,
  Storage,
  Attachment,
  TransferSrc,
  TransferDst,
  PresentSrc,
  Unknown,
};

extern std::unordered_map<std::pair<VkFormat, TextureType>, Ref<struct Texture>,
                          struct VkFormatTextureTypeHash>
    DefaultTextureCache; // NOLINT

auto UnloadModule() -> void;

struct Texture : Object, Barrier::BarrierSynced {
  std::mutex mutex;

  SamplerDescription samplerDescription{};
  VkExtent3D size{};

  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VmaAllocation memory = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  uint64_t sizeInBytes = 0;
  uint64_t lastUsedTimestamp{};

  VkFormat format = VK_FORMAT_UNDEFINED;
  size_t mipmapcount{};
  size_t arrayLayers{1};
  VkImageUsageFlags usage{};

  VkPipelineStageFlagBits2 lastPipelineStage =
      VK_PIPELINE_STAGE_NONE_KHR; // NOLINT

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
  auto UseDeferredDestruction() const -> bool override {
    return GetDeferredDestructionAllowed() && !isDestroyed;
  }

  Texture() = default;
  Texture(const Texture &) = delete;
  auto operator=(const Texture &) -> Texture & = delete;
  Texture(Texture &&) noexcept = delete;
  auto operator=(Texture &&) noexcept -> Texture & = delete;

  ~Texture() override {
    if (isSwapchainView) { // Not owned, don't destroy
      return;
    }

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    std::lock_guard<std::mutex> lock2(
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    auto *context = GetCurrentGraphicsContext();
    vkDestroyImageView(context->device, view, nullptr);
    vmaDestroyImage(context->vmaAllocator, image, memory);
  }

  enum TextureType textureType = TextureType::DEFAULT;

  auto SetFilter(VkFilter minFilter, VkFilter magFilter,
                 VkSamplerMipmapMode mipFilter) -> void;
  [[nodiscard]] auto GetFilter() const
      -> std::tuple<VkFilter, VkFilter, VkSamplerMipmapMode>;
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
  auto GetSampler(GraphicsContext &context) -> VkSampler;
  auto SetPixels(GraphicsContext &context, Image::ImageData &imageData,
                 uint32_t mipLevel, uint32_t arrayLayer, VkRect2D source,
                 VkOffset2D target) -> Error;
  auto SetPixels(GraphicsContext &context, Image::ImageData &imageData,
                 uint32_t mipLevel = 0, uint32_t arrayLayer = 0) -> Error;
  auto SetPixels(GraphicsContext &context, const std::span<const uint8_t> &data,
                 size_t width, size_t height, uint32_t mipLevel,
                 uint32_t arrayLayer, VkRect2D source, VkOffset2D target)
      -> Error;
  [[nodiscard]] auto GetMipmapCount() const -> size_t { return mipmapcount; }
  [[nodiscard]] auto GetFormat() const -> VkFormat { return format; }

  static auto GetType() -> Type const * { return &type; }
  auto TransitionLayout(
      const GraphicsContext &context, VkImageLayout layout,
      VkPipelineStageFlags2 sourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
                                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VkPipelineStageFlags2 destinationStage =
          VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VkAccessFlags2 srcAccessMask = VK_ACCESS_NONE, // NOLINT
      VkAccessFlags2 dstAccessMask = VK_ACCESS_NONE) -> Error;

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return Texture::GetType();
  }
};

struct TextureCreationInfo {
  uint32_t width = 0;  // Width in pixels
  uint32_t height = 0; // Height in pixels
  uint32_t depth{};    // Depth in pixels (for 3D textures, or Array layers)
  VkFormat format = VK_FORMAT_UNDEFINED; // Texture format
  VkImageUsageFlags usage{};             // Vulkan usage flags
  int mipmapCount{};                     // Number of mipmap levels
  std::string debugName;                 // Debug name
};

auto Create2D(const GraphicsContext &context, const TextureCreationInfo &info)
    -> Result<Ref<Texture>>;
auto FromSwapchainTexture(const GraphicsContext &context,
                          VkImage swapchainImage,
                          VkImageView swapchainImageView, VkFormat format,
                          uint32_t width, uint32_t height)
    -> Result<Ref<Texture>>;
auto CreateCubeMap(const GraphicsContext &context,
                   const TextureCreationInfo &info) -> Result<Ref<Texture>>;
auto CreateVolume(const GraphicsContext &context,
                  const TextureCreationInfo &info) -> Result<Ref<Texture>>;
auto CreateArray(const GraphicsContext &context,
                 const TextureCreationInfo &info) -> Result<Ref<Texture>>;

auto TransitionLayout(GraphicsContext &context, Texture *texture,
                      VkImageLayout oldLayout, VkImageLayout newLayout)
    -> Error;
auto CopyImageToBuffer(GraphicsContext &context, Texture *texture,
                       VkBuffer buffer) -> Error;
auto GenerateMipmaps(GraphicsContext &context, Texture *texture) -> Error;
auto LoadFromFile(GraphicsContext &context, const char *path,
                  VkImageUsageFlags usage = 0) -> Result<Ref<Texture>>;

// texture 2D From byte array
auto LoadFromMemory(GraphicsContext &context,
                    const std::span<const uint8_t> &data,
                    VkImageUsageFlags usage = 0) -> Result<Ref<Texture>>;

// texture 2D From ImageData
auto LoadFromMemory(GraphicsContext &context, Image::ImageData &imageData,
                    VkImageUsageFlags usage = 0) -> Result<Ref<Texture>>;

// texture 3D/Array/Cubemap From array of ImageData slices
auto LoadFromMemory(GraphicsContext &context,
                    const std::vector<Image::ImageData *> &slices,
                    TextureType type, VkImageUsageFlags usage = 0)
    -> Result<Ref<Texture>>;

auto GetDefaultTexture(GraphicsContext &context, VkFormat format,
                       Graphics::Texture::TextureType textureType)
    -> Result<Ref<Graphics::Texture::Texture>>;

auto GetAccessFlagsForUsage(TextureUsage usage, VkImageLayout currentLayout)
    -> VkAccessFlags2;

} // namespace Graphics::Texture
