#pragma once

#include "buffer.hpp"
#include "texture.hpp"
#include "vulkan/vulkan_core.h"
#include <variant>
#include <vector>

namespace Graphics::Rendergraph {

using ResourceHandle = uint16_t;

enum class ResourceLifetime : uint8_t {
  Transient, // Created and destroyed within a single frame, preffered
  Persistent // Created once and reused across frames
};

enum class AccessType : uint8_t { Read, Write };

struct ResourceAccess {
  ResourceHandle resource;
  AccessType accessType;
};

struct ResourceUsageLifetime {
  uint16_t firstUseIndex =
      UINT16_MAX;            // First use in the compiled render pass sequence
  uint16_t lastUseIndex = 0; // Last use in the compiled render pass sequence
};

struct TextureResource {
  VkFormat format;
  VkExtent3D extent;
  uint32_t mipLevels;
  uint32_t arrayLayers;
  VkImageUsageFlags usage;
  VkSampleCountFlagBits samples;
};

struct BufferResource {
  VkDeviceSize size;
  VkBufferUsageFlags usage;
};

struct ImportedTexture {
  Graphics::Texture::Texture texture;

  VkImageLayout initialLayout;
  VkImageLayout finalLayout;
};

struct ImportedBuffer {
  Graphics::Buffer buffer;
};

struct TextureInfo {
  bool imported;
  TextureResource transient;
  ImportedTexture external;
};

struct BufferInfo {
  bool imported;
  BufferResource transient;
  ImportedBuffer external;
};

enum class Type : uint8_t { Texture, Buffer, Unknown };

struct Resource {
  ResourceHandle handle = 0;
  ResourceLifetime lifetime = ResourceLifetime::Transient;
  ResourceUsageLifetime usageLifetime = {};
  uint32_t cost = 0; // Estimated cost, e.g. memory size in bytes, used to
                     // optimize for least cost

  Type type = Type::Unknown;
  bool imported = false;

  std::variant<TextureInfo, BufferInfo> info;
};

struct RenderPass {
  ResourceHandle handle;

  std::vector<ResourceHandle> readResources;
  std::vector<ResourceHandle> writeResources;

  std::vector<ResourceHandle> parents;
  std::vector<ResourceHandle> children;
};

struct CompiledPass {
  RenderPass pass;

  std::vector<ResourceHandle> barriersBefore;
  std::vector<ResourceHandle> barriersAfter;

  std::vector<ResourceHandle> children;
};

enum class RenderGraphHeuristic : uint8_t {
  SmallestResourceFirst, // Reduce peak memory usage
  LargestResourceFirst,  // Increase aliasing opportunities
  MostChildrenFirst,     // Increase parallelism
  LeastChildrenFirst     // Reduce synchronization overhead
};

struct RenderGraph {
  std::vector<Resource> resources;
  std::vector<RenderPass> passes;

  // Virtual root node to allow for multiple root passes
  CompiledPass virtualRoot;

  // Should store [index == handle] -> CompiledPass mapping
  std::vector<CompiledPass> compiledPasses;

  RenderGraphHeuristic heuristic = RenderGraphHeuristic::LargestResourceFirst;
};

struct TextureDescriptor {
  Texture::TextureType type = Texture::TextureType::TEXTURE_TYPE_2D;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

  uint32_t width{};
  uint32_t height{};
  uint32_t depthOrLayers = 1; // depth for 3D, layers for array/cube
  uint32_t mipLevels = 1;

  // Sampling / filtering
  VkFilter minFilter = VK_FILTER_LINEAR;
  VkFilter magFilter = VK_FILTER_LINEAR;
  VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  float anisotropy = 1.0F;

  ResourceLifetime lifetime = ResourceLifetime::Transient;

  // Vulkan usage flags (required)
  VkImageUsageFlags usage = 0;

  // Optional: views/layouts if you want custom behavior
  VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
};

struct BufferDescriptor {
  VkDeviceSize size = 0;            // total buffer size in bytes
  VkBufferUsageFlags usage = 0;     // Vulkan usage flags
  VkMemoryPropertyFlags memory = 0; // device-local, host-visible, etc.

  ResourceLifetime lifetime = ResourceLifetime::Transient;

  bool allowAliasing = true;
};

auto AddTexture(RenderGraph &graph, TextureDescriptor descriptor)
    -> ResourceHandle;

auto AddBuffer(RenderGraph &graph, BufferDescriptor descriptor)
    -> ResourceHandle;

auto ImportTexture(
    RenderGraph &graph, const Graphics::Texture::Texture &texture,
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    -> ResourceHandle;

auto ImportBuffer(RenderGraph &graph, const Graphics::Buffer &buffer)
    -> ResourceHandle;

auto AddRenderPass(RenderGraph &graph,
                   const std::vector<ResourceAccess> &resourceAccesses)
    -> ResourceHandle;

auto Compile(RenderGraph &graph) -> void;

} // namespace Graphics::Rendergraph