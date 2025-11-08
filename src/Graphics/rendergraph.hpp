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

  RenderGraphHeuristic heuristic = RenderGraphHeuristic::SmallestResourceFirst;
};

auto AddTexture(RenderGraph &graph, VkFormat format, VkExtent3D extent,
                VkImageUsageFlags usage,
                ResourceLifetime lifetime = ResourceLifetime::Transient)
    -> ResourceHandle;

auto AddBuffer(RenderGraph &graph, VkDeviceSize size, VkBufferUsageFlags usage,
               ResourceLifetime lifetime = ResourceLifetime::Transient)
    -> ResourceHandle;

auto ImportTexture(
    RenderGraph &graph, const Graphics::Texture::Texture &texture,
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    -> ResourceHandle;

auto ImportBuffer(RenderGraph &graph, const Graphics::Buffer &buffer)
    -> ResourceHandle;

auto AddRenderPass(RenderGraph &graph,
                   const std::vector<ResourceAccess> &resourceAccesses) -> void;

auto Compile(RenderGraph &graph) -> void;

} // namespace Graphics::Rendergraph