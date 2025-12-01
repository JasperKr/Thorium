#pragma once

#include "Modules/error.hpp"
#include "buffer.hpp"
#include "graphics.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include <unordered_map>
#include <variant>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <vector>

namespace Graphics {
namespace Rendergraph {

using ResourceHandle = uint16_t;

auto inline GetSamplerCache()
    -> std::unordered_map<ResourceHandle, VkSampler> & {
  static std::unordered_map<ResourceHandle, VkSampler> samplerCache = {};
  return samplerCache;
}

enum class ResourceLifetime : uint8_t {
  Transient, // Created and destroyed within a single frame, preffered
  Persistent // Created once and reused across frames
};

// Access type for a resource within a render pass
// Bit-Op combinable, for read-write resources
// NOLINTNEXTLINE, because of 32-bit enum usage for values < 256
enum AccessType : uint32_t { Read = 1U << 0U, Write = 1U << 1U };

struct ResourceAccess {
  ResourceHandle resource;
  AccessType accessType;
};

struct ResourceUsageLifetime {
  uint16_t firstUseIndex =
      UINT16_MAX;            // First use in the compiled render pass sequence
  uint16_t lastUseIndex = 0; // Last use in the compiled render pass sequence
};

struct ImportedTexture {
  Graphics::Texture::Texture texture;

  VkImageLayout initialLayout;
  VkImageLayout finalLayout;
};

struct ImportedBuffer {
  Graphics::Buffer buffer;
};

enum class Type : uint8_t { Texture, Buffer, Unknown };

enum class ResourceUsage : uint8_t { ReadOnly, WriteOnly, ReadWrite };

enum class BindingType : uint8_t {
  Sampler,   // Sampler
  Storage,   // SSBO / image
  Uniform,   // UBO
  Attachment // Color / depth attachment
};

struct ResourceBinding {
  ResourceHandle resource = UINT16_MAX;
  uint32_t binding = 0;
  uint32_t set = 0;

  // Must be set to UINT32_MAX if not used
  // Is only used for framebuffer attachments
  uint32_t location = UINT32_MAX;

  // Vertex / Fragment / Compute. Vertex and Fragment can be combined.
  VkShaderStageFlags stageFlags =
      static_cast<uint32_t>(VK_SHADER_STAGE_FRAGMENT_BIT) |
      static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT);

  BindingType type = BindingType::Sampler;
  ResourceUsage usage = ResourceUsage::ReadOnly;

  // User-friendly key for changing Persistent resource handles
  std::string name;
};

struct Resource {
  ResourceHandle handle = 0;
  ResourceLifetime lifetime = ResourceLifetime::Transient;
  ResourceUsageLifetime usageLifetime = {};
  uint32_t cost = 0; // Estimated cost, e.g. memory size in bytes, used to
                     // optimize for least cost

  Type type = Type::Unknown;

  std::variant<Texture::Texture, Buffer> info{};
};

// struct DescriptorSetUpdate {
//   std::string name;
//   ResourceHandle newResource;
// };

struct PassState {
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

  std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;
  std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets;

  VkViewport viewport = {};
  VkRect2D scissor = {};

  std::vector<VkClearValue> clearValues;
  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  std::vector<VkPipelineColorBlendAttachmentState> blendModes;
  VkSubpassDescription subpassDescription = {};

  std::vector<VkWriteDescriptorSet> descriptorWrites;
};

struct RenderPass {
  ResourceHandle handle;

  std::vector<ResourceHandle> readResources;
  std::vector<ResourceHandle> writeResources;
  std::vector<ResourceHandle> readwriteResources;

  std::vector<ResourceHandle> parents;
  std::vector<ResourceHandle> children;

  PassState state = {};
  std::vector<ResourceBinding> resourceBindings;

  [[nodiscard]] auto GetResources(AccessType accessType) const
      -> std::vector<ResourceHandle> {
    std::vector<ResourceHandle> resources;

    auto read = static_cast<uint32_t>(AccessType::Read);
    auto write = static_cast<uint32_t>(AccessType::Write);
    uint32_t readwrite = read | write;

    if (accessType == read) {
      resources.insert(resources.end(), readResources.begin(),
                       readResources.end());
      resources.insert(resources.end(), readwriteResources.begin(),
                       readwriteResources.end());
    }

    if (accessType == write) {
      resources.insert(resources.end(), writeResources.begin(),
                       writeResources.end());
      resources.insert(resources.end(), readwriteResources.begin(),
                       readwriteResources.end());
    }

    if (accessType == readwrite) {
      resources.insert(resources.end(), readwriteResources.begin(),
                       readwriteResources.end());
      resources.insert(resources.end(), readResources.begin(),
                       readResources.end());
      resources.insert(resources.end(), writeResources.begin(),
                       writeResources.end());
    }

    return resources;
  }

  Ref<Shader::ShaderModule> shader;

  std::function<void(VkCommandBuffer cmd, GraphicsContext &context,
                     struct RenderGraph &graph,
                     struct CompiledPass &currentPass)>
      executeFunction;
};

enum class ResourceTimelineEntryType : uint8_t { Allocate, Deallocate };

struct ResourceTimelineEntry {
  ResourceHandle resourceHandle;  // Associated resource handle
  ResourceHandle passHandle;      // Associated pass handle
  ResourceTimelineEntryType type; // Type of event (Allocate or Deallocate)
};

struct LayoutState {
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkPipelineStageFlags stages = 0;
  VkAccessFlags access = 0;
};

struct LayoutUpdate {
  ResourceHandle resource{}; // Resource to update

  LayoutState oldState; // Previous layout state
  LayoutState newState; // New layout state
};

struct CompiledPass {
  RenderPass pass;

  std::vector<ResourceHandle> barriersBefore;
  std::vector<ResourceHandle> barriersAfter;

  std::vector<ResourceHandle> children;

  // Separated allocation/deallocation entries, for easier processing in runtime
  std::vector<ResourceTimelineEntry> allocations;
  std::vector<ResourceTimelineEntry> deallocations;

  std::vector<LayoutUpdate> layoutUpdates;
  // Used to generate layout transitions
  std::unordered_map<ResourceHandle, LayoutState> resourceLayouts;
  std::vector<VkImageMemoryBarrier> imageBarriers;
};

enum class RenderGraphHeuristic : uint8_t {
  SmallestResourceFirst, // Reduce peak memory usage
  LargestResourceFirst,  // Increase aliasing opportunities
  MostChildrenFirst,     // Increase parallelism
  LeastChildrenFirst     // Reduce synchronization overhead
};

const VkDeviceSize KiB = 1024;
const VkDeviceSize MiB = 1024 * KiB;
const VkDeviceSize DefaultMemoryBlockSize = 16 * MiB;

struct MemoryBlock {
  VkDeviceSize size = 0;
  VkDeviceSize offset = 0;
  VmaVirtualBlock virtualBlock = nullptr;
  VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct VirtualAllocation {
  uint32_t blockIndex;     // index of the memory block
  ResourceHandle resource; // associated resource handle
  VmaVirtualAllocation allocation;
  VkDeviceSize offset; // offset within the memory block
  VkDeviceSize size;   // size of the allocation
};

struct RenderGraph {
  std::vector<Resource> resources;
  std::vector<RenderPass> passes;

  // Virtual root node to allow for multiple root passes
  CompiledPass virtualRoot;

  // Should store [index == handle] -> CompiledPass mapping
  std::vector<CompiledPass> compiledPasses;
  std::vector<ResourceTimelineEntry> compiledResources;

  RenderGraphHeuristic heuristic = RenderGraphHeuristic::LargestResourceFirst;
  VkDeviceSize memoryBlockSize = DefaultMemoryBlockSize;

  std::vector<MemoryBlock> memoryBlocks;
  std::unordered_map<ResourceHandle, VirtualAllocation> virtualAllocations;

  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

  // Persistent resources only
  std::vector<VkImageMemoryBarrier> postGraphUpdates;
  // Persistent resources only
  std::unordered_map<ResourceHandle, LayoutState> initialResourceLayouts;
  // Persistent resources only
  std::unordered_map<ResourceHandle, LayoutState> finalResourceLayouts;
};

struct TextureDescriptor {
  Texture::TextureType type = Texture::TextureType::DEFAULT;
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

auto AddTexture(RenderGraph &graph, const TextureDescriptor &descriptor)
    -> ResourceHandle;

auto AddBuffer(RenderGraph &graph, const BufferDescriptor &descriptor)
    -> ResourceHandle;

auto ImportTexture(RenderGraph &graph, Graphics::Texture::Texture texture,
                   LayoutUpdate layoutUpdate) -> ResourceHandle;

auto ImportBuffer(RenderGraph &graph, const Graphics::Buffer &buffer)
    -> ResourceHandle;

struct RenderPassDescriptor {
  std::vector<ResourceHandle> resources;

  VkViewport viewport = {};
  VkRect2D scissor = {};

  // Clear values for attachments, optional.
  std::vector<VkClearValue> clearValues;
  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  // Optional, defaults to overwrite
  std::vector<VkPipelineColorBlendAttachmentState> blendModes;
  std::vector<ResourceBinding> resourceBindings;

  Ref<Graphics::Shader::ShaderModule> shader;

  std::function<void(VkCommandBuffer cmd, GraphicsContext &context,
                     struct RenderGraph &graph, CompiledPass &currentPass)>
      executeFunction;
};

auto AddRenderPass(RenderGraph &graph, const RenderPassDescriptor &descriptor)
    -> ResourceHandle;

[[nodiscard]] auto Compile(GraphicsContext &context, RenderGraph &graph)
    -> Error::Error;

auto Execute(GraphicsContext &context, RenderGraph &graph,
             VkCommandBuffer commandBuffer) -> void;

} // namespace Rendergraph
} // namespace Graphics