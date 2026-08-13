#pragma once

#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/error.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {

// 65K events per frame should be more than enough
// Loading bistro in a single frame takes < 15K events, so it should be fine.
using CommandID = uint16_t;
static constexpr uint16_t InvalidCommandID = UINT16_MAX;

// We have a virtual command buffer per-thread
// Each command buffer stores the thread's recorded commands,
// later, they will be combined in to a final large frame virtual command buffer
// a DAG will be built and barriers will be inserted.

// A virtual command buffer will be one-time use and have some userdata;
// I want to support different queue families so a vcmd must store which queue family it will be queued to.
// These command buffers are NOT thread-safe. Must either be externally synchronised, or just have 1 copy per-thread.
// only one vk command buffer will be associated per vcmd, or even less than one if one can span multiple (for example, when queue families match)

// This means commands do not need to reference the command buffer at recording time.

enum class CommandType : uint8_t {
  vkCmdDraw,
  vkCmdDrawIndexed,
  vkCmdDrawIndirect,
  vkCmdDrawIndexedIndirect,
  vkCmdDispatch,
  vkCmdDispatchIndirect,
  vkCmdBlitImage,

  vkCmdCopyBuffer,
  vkCmdCopyImage,
  vkCmdCopyBufferToImage,
  vkCmdCopyImageToBuffer,

  mipmapTexture,
  vkCmdFillBuffer,

  vkCmdBuildAccelerationStructuresKHR,
  vkCmdCopyAccelerationStructureKHR,
  vkCmdResetQueryPool,
  vkCmdWriteAccelerationStructuresPropertiesKHR,

  vkCmdClearAttachments,
};

static const Utils::EnumStringHelper<CommandType> CommandTypeEnumHelper{{
    "vkCmdDraw",
    "vkCmdDrawIndexed",
    "vkCmdDrawIndirect",
    "vkCmdDrawIndexedIndirect",
    "vkCmdDispatch",
    "vkCmdDispatchIndirect",
    "vkCmdBlitImage",

    "vkCmdCopyBuffer",
    "vkCmdCopyImage",
    "vkCmdCopyBufferToImage",
    "vkCmdCopyImageToBuffer",

    "mipmapTexture",
    "vkCmdFillBuffer",

    "vkCmdBuildAccelerationStructuresKHR",
    "vkCmdCopyAccelerationStructureKHR",
    "vkCmdResetQueryPool",
    "vkCmdWriteAccelerationStructuresPropertiesKHR",

    "vkCmdClearAttachments",
}};

struct GraphState {
  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
  VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
  VkBool32 depthTestEnable = 1;
  VkBool32 depthWriteEnable = 1;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
  VkBool32 stencilTestEnable = 0;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  VkViewport viewport{};
  VkRect2D scissor{};

  std::vector<VkBuffer> vertexBuffers;
  std::vector<VkDeviceSize> vertexBufferOffsets;
  VkBuffer indexBuffer = VK_NULL_HANDLE;
  VkIndexType indexType = VK_INDEX_TYPE_MAX_ENUM;
  VkDeviceSize indexBufferOffset{};

  mutable bool dirty = true;

  std::string currentDebugMarker;
  std::optional<Color> currentDebugMarkerColor;

  std::vector<VkColorBlendEquationEXT> colorBlendEquations;

  Ref<Shader> shader;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  std::vector<DynamicRendering::RenderTarget> colorAttachments;
  DynamicRendering::RenderTarget depthStencilAttachment;
  bool hasDepthStencilAttachment = false;

  std::vector<VkBool32> blendEnables;
  std::vector<VkColorComponentFlags> colorWriteMasks;

  std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;

  // Not taken into account for hashing, purely for copying to per-draw state
  std::vector<char> pushConstants;

  mutable uint64_t hash{};

  // Incremented each time the state is modified
  mutable uint64_t generation = 0;

  void MarkUpdated() {
    generation++;
    dirty = true;
  }

  auto GetHash() const -> uint64_t;

  auto operator==(const GraphState &other) const -> bool {
    [[likely]]
    if (generation == other.generation) { // quick equal
      return true;
    }

    if (colorAttachments != other.colorAttachments) {
      return false;
    }

    if (shader != other.shader) {
      return false;
    }

    if (stencilTestEnable != other.stencilTestEnable ||
        polygonMode != other.polygonMode ||
        primitiveTopology != other.primitiveTopology ||
        bindPoint != other.bindPoint) {
      return false;
    }

    if (hasDepthStencilAttachment != other.hasDepthStencilAttachment) {
      return false;
    }

    if (hasDepthStencilAttachment) {
      if (depthStencilAttachment != other.depthStencilAttachment) {
        return false;
      }
    }

    if (blendEnables != other.blendEnables ||
        colorWriteMasks != other.colorWriteMasks) {
      return false;
    }

    return true;
  }

  auto ToString() const -> std::string;
};

struct GraphStateHash {
  auto operator()(const GraphState &state) const -> uint64_t {
    return state.GetHash();
  };
};

struct CommandStateManager {
  static std::unordered_map<GraphState, uint32_t, GraphStateHash> StateToIndex;
  static std::vector<GraphState> States;
};

// NOLINTBEGIN(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTBEGIN(hicpp-explicit-conversions)

struct Callable {
  virtual ~Callable() = default;
  virtual auto Call(VkCommandBuffer cmdBuffer) const -> Error = 0;
};

struct BoundResources {
  std::vector<void *> reads;
  std::vector<void *> writes;
};

struct BoundImage {
  VkImage image;
  SlangResourceAccess access;
};

struct BoundBuffer {
  VkBuffer buffer;
  SlangResourceAccess access;
};

struct DrawState {
  // When the read / write state is saved, take in to account that
  // Texture blits for mipmapping read / write the same texture but different views
  // And must have an exception made

  std::vector<BoundImage> boundImages;
  std::vector<BoundBuffer> boundBuffers;
  std::vector<VkAccelerationStructureKHR> boundAccelerationStructures;

  // no need for a BoundImage, since usage is implied.
  std::vector<VkImage> colorAttachments;
  VkImage depthStencilAttachment;

  std::vector<VkBuffer> vertexBuffers;
  VkBuffer indexBuffer = VK_NULL_HANDLE;

  // Copied from graph state.
  std::vector<char> pushConstants;

  uint32_t stateID;

  DrawState();
};

namespace Args {

struct VkCmdDraw : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDraw;

  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;

  VkCmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance)
      : vertexCount(vertexCount), instanceCount(instanceCount),
        firstVertex(firstVertex), firstInstance(firstInstance) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex,
              firstInstance);
    return {};
  }
};

struct VkCmdDrawIndexed : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndexed;

  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;

  VkCmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                   uint32_t firstIndex, int32_t vertexOffset,
                   uint32_t firstInstance)
      : indexCount(indexCount), instanceCount(instanceCount),
        firstIndex(firstIndex), vertexOffset(vertexOffset),
        firstInstance(firstInstance) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex,
                     vertexOffset, firstInstance);
    return {};
  }
};

struct VkCmdDrawIndirect : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  VkCmdDrawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                    uint32_t stride)
      : buffer(buffer), offset(offset), drawCount(drawCount), stride(stride) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndirect(cmdBuffer, buffer, offset, drawCount, stride);
    return {};
  }
};

struct VkCmdDrawIndexedIndirect : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndexedIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  VkCmdDrawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset,
                           uint32_t drawCount, uint32_t stride)
      : buffer(buffer), offset(offset), drawCount(drawCount), stride(stride) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndexedIndirect(cmdBuffer, buffer, offset, drawCount, stride);
    return {};
  }
};

struct VkCmdDispatch : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDispatch;

  uint32_t groupCountX;
  uint32_t groupCountY;
  uint32_t groupCountZ;

  VkCmdDispatch(uint32_t groupCountX, uint32_t groupCountY,
                uint32_t groupCountZ)
      : groupCountX(groupCountX), groupCountY(groupCountY),
        groupCountZ(groupCountZ) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);
    return {};
  }
};

struct VkCmdDispatchIndirect : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDispatchIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;

  VkCmdDispatchIndirect(VkBuffer buffer, VkDeviceSize offset)
      : buffer(buffer), offset(offset) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDispatchIndirect(cmdBuffer, buffer, offset);
    return {};
  }
};

struct VkCmdBlitImage : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdBlitImage;

  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  std::vector<VkImageBlit> regions;
  VkFilter filter;

  VkCmdBlitImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageBlit *pRegions,
                 VkFilter filter)
      : srcImage(srcImage), srcImageLayout(srcImageLayout), dstImage(dstImage),
        dstImageLayout(dstImageLayout), filter(filter),
        regions(pRegions, pRegions + regionCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdBlitImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data(), filter);
    return {};
  }
};

struct VkCmdPushConstants {
  VkPipelineLayout layout;
  VkShaderStageFlags stageFlags;
  uint32_t offset;
  std::vector<char> values;

  VkCmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                     uint32_t offset, uint32_t size, const void *pValues)
      : layout(layout), stageFlags(stageFlags), offset(offset),
        values(static_cast<const char *>(pValues),
               static_cast<const char *>(pValues) + size) {}
};

struct VkCmdCopyBuffer : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyBuffer;

  VkBuffer srcBuffer;
  VkBuffer dstBuffer;
  std::vector<VkBufferCopy> regions;

  VkCmdCopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                  const VkBufferCopy *pRegions)
      : srcBuffer(srcBuffer), dstBuffer(dstBuffer),
        regions(pRegions, pRegions + regionCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, regions.size(),
                    regions.data());
    return {};
  }
};

struct VkCmdCopyImage : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyImage;

  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  std::vector<VkImageCopy> regions;

  VkCmdCopyImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageCopy *pRegions)
      : srcImage(srcImage), srcImageLayout(srcImageLayout), dstImage(dstImage),
        dstImageLayout(dstImageLayout),
        regions(pRegions, pRegions + regionCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data());
    return {};
  }
};

struct VkCmdCopyBufferToImage : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyBufferToImage;

  VkBuffer srcBuffer;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  std::vector<VkBufferImageCopy> regions;

  VkCmdCopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage,
                         VkImageLayout dstImageLayout, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions)
      : srcBuffer(srcBuffer), dstImage(dstImage),
        dstImageLayout(dstImageLayout),
        regions(pRegions, pRegions + regionCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage, dstImageLayout,
                           regions.size(), regions.data());
    return {};
  }
};

struct VkCmdCopyImageToBuffer : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyImageToBuffer;

  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkBuffer dstBuffer;
  std::vector<VkBufferImageCopy> regions;

  VkCmdCopyImageToBuffer(VkImage srcImage, VkImageLayout srcImageLayout,
                         VkBuffer dstBuffer, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions)
      : srcImage(srcImage), srcImageLayout(srcImageLayout),
        dstBuffer(dstBuffer), regions(pRegions, pRegions + regionCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, dstBuffer,
                           regions.size(), regions.data());
    return {};
  }
};

struct MipmapTexture : Callable, BoundResources {
  static const CommandType type = CommandType::mipmapTexture;

  Texture *texture;

  MipmapTexture(Texture *texture) : texture(texture) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    return texture->GenerateMipmaps(*GetCurrentGraphicsContext(), cmdBuffer);
  }
};

struct VkCmdFillBuffer : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdFillBuffer;

  VkBuffer dstBuffer;
  VkDeviceSize dstOffset;
  VkDeviceSize size;
  uint32_t data;

  VkCmdFillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size,
                  uint32_t data)
      : dstBuffer(dstBuffer), dstOffset(dstOffset), size(size), data(data) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdFillBuffer(cmdBuffer, dstBuffer, dstOffset, size, data);
    return {};
  }
};

struct VkCmdBuildAccelerationStructuresKHR : Callable {
  static const CommandType type =
      CommandType::vkCmdBuildAccelerationStructuresKHR;

  uint32_t infoCount;
  std::vector<VkAccelerationStructureBuildGeometryInfoKHR> infos;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;

  VkCmdBuildAccelerationStructuresKHR(
      uint32_t infoCount,
      const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
      const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
      : infoCount(infoCount), infos(pInfos, pInfos + infoCount), // NOLINT
        buildRangeInfos(infoCount) {
    for (size_t i = 0; i < infoCount; ++i) {
      assert(infos.at(i).pNext == nullptr);
      buildRangeInfos[i] = *ppBuildRangeInfos[i]; // NOLINT
    }
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR *>
        ppBuildRangeInfos;
    ppBuildRangeInfos.reserve(buildRangeInfos.size());
    for (const auto &range : buildRangeInfos) {
      ppBuildRangeInfos.push_back(&range);
    }
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, infoCount, infos.data(),
                                        ppBuildRangeInfos.data());
    return {};
  }
};

struct VkCmdCopyAccelerationStructureKHR : Callable {
  static const CommandType type =
      CommandType::vkCmdCopyAccelerationStructureKHR;

  VkCopyAccelerationStructureInfoKHR structureInfo;

  VkCmdCopyAccelerationStructureKHR(
      const VkCopyAccelerationStructureInfoKHR *pInfo)
      : structureInfo(*pInfo) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyAccelerationStructureKHR(cmdBuffer, &structureInfo);
    return {};
  }
};

struct VkCmdResetQueryPool : Callable {
  static const CommandType type = CommandType::vkCmdResetQueryPool;

  VkQueryPool queryPool;
  uint32_t firstQuery;
  uint32_t queryCount;

  VkCmdResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery,
                      uint32_t queryCount)
      : queryPool(queryPool), firstQuery(firstQuery), queryCount(queryCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdResetQueryPool(cmdBuffer, queryPool, firstQuery, queryCount);
    return {};
  }
};

struct VkCmdWriteAccelerationStructuresPropertiesKHR : Callable {
  static const CommandType type =
      CommandType::vkCmdWriteAccelerationStructuresPropertiesKHR;

  std::vector<VkAccelerationStructureKHR> accelerationStructures;
  VkQueryType queryType;
  VkQueryPool queryPool;
  uint32_t firstQuery;

  VkCmdWriteAccelerationStructuresPropertiesKHR(
      uint32_t accelerationStructureCount,
      const VkAccelerationStructureKHR *pAccelerationStructures,
      VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
      : accelerationStructures(pAccelerationStructures,
                               pAccelerationStructures +
                                   accelerationStructureCount),
        queryType(queryType), queryPool(queryPool), firstQuery(firstQuery) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdWriteAccelerationStructuresPropertiesKHR(
        cmdBuffer, accelerationStructures.size(), accelerationStructures.data(),
        queryType, queryPool, firstQuery);
    return {};
  }
};

struct VkCmdBindIndexBuffer {
  VkBuffer buffer;
  VkDeviceSize offset;
  VkIndexType indexType;

  VkCmdBindIndexBuffer(VkBuffer buffer, VkDeviceSize offset,
                       VkIndexType indexType)
      : buffer(buffer), offset(offset), indexType(indexType) {}
};

struct VkCmdBindVertexBuffers {
  uint32_t firstBinding;
  std::vector<VkBuffer> buffers;
  std::vector<VkDeviceSize> offsets;

  VkCmdBindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount,
                         const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
      : firstBinding(firstBinding), buffers(pBuffers, pBuffers + bindingCount),

        offsets(pOffsets, pOffsets + bindingCount) {}
};

struct VkCmdSetVertexInputEXT {
  std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;

  VkCmdSetVertexInputEXT(
      uint32_t vertexBindingDescriptionCount,
      const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
      uint32_t vertexAttributeDescriptionCount,
      const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
      : bindingDescriptions(pVertexBindingDescriptions,
                            pVertexBindingDescriptions +
                                vertexBindingDescriptionCount),
        attributeDescriptions(pVertexAttributeDescriptions,
                              pVertexAttributeDescriptions +
                                  vertexAttributeDescriptionCount) {}
};

struct VkCmdBindPipeline {
  VkPipelineBindPoint pipelineBindPoint;
  VkPipeline pipeline;

  VkCmdBindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
      : pipelineBindPoint(pipelineBindPoint), pipeline(pipeline) {}
};

struct VkCmdBindDescriptorSets {
  VkPipelineBindPoint pipelineBindPoint;
  VkPipelineLayout layout;
  uint32_t firstSet;
  std::vector<VkDescriptorSet> descriptorSets;
  std::vector<uint32_t> dynamicOffsets;

  VkCmdBindDescriptorSets(VkPipelineBindPoint pipelineBindPoint,
                          VkPipelineLayout layout, uint32_t firstSet,
                          uint32_t descriptorSetCount,
                          const VkDescriptorSet *pDescriptorSets,
                          uint32_t dynamicOffsetCount,
                          const uint32_t *pDynamicOffsets)
      : pipelineBindPoint(pipelineBindPoint), layout(layout),
        firstSet(firstSet),
        descriptorSets(pDescriptorSets, pDescriptorSets + descriptorSetCount),

        dynamicOffsets(pDynamicOffsets, pDynamicOffsets + dynamicOffsetCount) {}
};

struct VkCmdSetViewport {
  uint32_t firstViewport;
  std::vector<VkViewport> viewports;

  VkCmdSetViewport(uint32_t firstViewport, uint32_t viewportCount,
                   const VkViewport *pViewports)
      : firstViewport(firstViewport),
        viewports(pViewports, pViewports + viewportCount) {}
};

struct VkCmdSetScissor {
  uint32_t firstScissor;
  std::vector<VkRect2D> scissors;

  VkCmdSetScissor(uint32_t firstScissor, uint32_t scissorCount,
                  const VkRect2D *pScissors)
      : firstScissor(firstScissor),
        scissors(pScissors, pScissors + scissorCount) {}
};

struct VkCmdSetDepthTestEnable {
  VkBool32 depthTestEnable;

  VkCmdSetDepthTestEnable(VkBool32 depthTestEnable)
      : depthTestEnable(depthTestEnable) {}
};

struct VkCmdSetDepthWriteEnable {
  VkBool32 depthWriteEnable;

  VkCmdSetDepthWriteEnable(VkBool32 depthWriteEnable)
      : depthWriteEnable(depthWriteEnable) {}
};

struct VkCmdSetDepthCompareOp {
  VkCompareOp depthCompareOp;

  VkCmdSetDepthCompareOp(VkCompareOp depthCompareOp)
      : depthCompareOp(depthCompareOp) {}
};

struct VkCmdSetColorBlendEquationEXT {
  uint32_t firstAttachment;
  std::vector<VkColorBlendEquationEXT> equations;

  VkCmdSetColorBlendEquationEXT(
      uint32_t firstAttachment, uint32_t attachmentCount,
      const VkColorBlendEquationEXT *pColorBlendEquations)
      : firstAttachment(firstAttachment),
        equations(pColorBlendEquations,
                  pColorBlendEquations + attachmentCount) {}
};

struct VkCmdSetColorBlendEnableEXT {
  uint32_t firstAttachment;
  std::vector<VkBool32> enables;

  VkCmdSetColorBlendEnableEXT(uint32_t firstAttachment,
                              uint32_t attachmentCount,
                              const VkBool32 *pColorBlendEnables)
      : firstAttachment(firstAttachment),
        enables(pColorBlendEnables, pColorBlendEnables + attachmentCount) {}
};

struct VkCmdSetColorWriteMaskEXT {
  uint32_t firstAttachment;
  std::vector<VkColorComponentFlags> masks;

  VkCmdSetColorWriteMaskEXT(uint32_t firstAttachment, uint32_t attachmentCount,
                            const VkColorComponentFlags *pColorWriteMasks)
      : firstAttachment(firstAttachment),
        masks(pColorWriteMasks, pColorWriteMasks + attachmentCount) {}
};

struct VkCmdSetCullMode {
  VkCullModeFlags cullMode;

  VkCmdSetCullMode(VkCullModeFlags cullMode) : cullMode(cullMode) {}
};

struct VkCmdSetFrontFace {
  VkFrontFace frontFace;

  VkCmdSetFrontFace(VkFrontFace frontFace) : frontFace(frontFace) {}
};

struct VkCmdClearAttachments : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdClearAttachments;

  std::vector<VkClearAttachment> attachments;
  std::vector<VkClearRect> rects;

  VkCmdClearAttachments(uint32_t attachmentCount,
                        const VkClearAttachment *pAttachments,
                        uint32_t rectCount, const VkClearRect *pRects)
      : attachments(pAttachments, pAttachments + attachmentCount),
        rects(pRects, pRects + rectCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdClearAttachments(cmdBuffer, attachments.size(), attachments.data(),
                          rects.size(), rects.data());
    return {};
  }
};

struct VkCmdBeginDebugUtilsLabelEXT {
  VkDebugUtilsLabelEXT labelInfo;

  VkCmdBeginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT *pLabelInfo)
      : labelInfo(*pLabelInfo) {}
};

struct VkCmdEndDebugUtilsLabelEXT {};

struct VkCmdInsertDebugUtilsLabelEXT {
  VkDebugUtilsLabelEXT labelInfo;

  VkCmdInsertDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT *pLabelInfo)
      : labelInfo(*pLabelInfo) {}
};

// NOLINTEND(hicpp-explicit-conversions)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTEND(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
} // namespace Args

template <typename T, typename... Ts>
auto get_if_derived(std::variant<Ts...> &variant) -> T * {
  T *result = nullptr;

  std::visit(
      [&](auto &value) -> auto {
        using U = std::remove_cvref_t<decltype(value)>;

        if constexpr (std::derived_from<U, T>) {
          result = &value;
        }
      },
      variant);

  return result;
}

template <typename T, typename... Ts>
auto get_if_derived(const std::variant<Ts...> &variant) -> T const * {
  T const *result = nullptr;

  std::visit(
      [&](auto &value) -> auto {
        using U = std::remove_cvref_t<decltype(value)>;

        if constexpr (std::derived_from<U, T>) {
          result = &value;
        }
      },
      variant);

  return result;
}

using ArgVariants = std::variant<
    Args::VkCmdDraw, Args::VkCmdDrawIndexed, Args::VkCmdDrawIndirect,
    Args::VkCmdDrawIndexedIndirect, Args::VkCmdDispatch,
    Args::VkCmdDispatchIndirect, Args::VkCmdBlitImage, Args::VkCmdCopyBuffer,
    Args::VkCmdCopyImage, Args::VkCmdCopyBufferToImage,
    Args::VkCmdCopyImageToBuffer, Args::MipmapTexture, Args::VkCmdFillBuffer,
    Args::VkCmdBuildAccelerationStructuresKHR,
    Args::VkCmdCopyAccelerationStructureKHR, Args::VkCmdResetQueryPool,
    Args::VkCmdWriteAccelerationStructuresPropertiesKHR,
    Args::VkCmdClearAttachments>;

struct Command {
  CommandID id = InvalidCommandID;

  // Also defined in framegraph.cpp as InvalidDepth
  CommandID level = UINT16_MAX;

  ArgVariants data;

  explicit Command(ArgVariants params) : data(std::move(params)) {}

  [[nodiscard]] auto GetType() const -> CommandType {
    return std::visit(
        [](const auto &current) -> CommandType { return current.type; }, data);
  }

  [[nodiscard]] auto GetBoundResources() -> DrawState * {
    return get_if_derived<DrawState>(data);
  }

  [[nodiscard]] auto GetBoundResources() const -> DrawState const * {
    return get_if_derived<DrawState>(data);
    return {};
  }
};

struct BufferStateUpdate {
  VkBuffer buffer;
  ResourceState state;
  CommandID time;
};

struct ImageStateUpdate {
  VkImage image;
  ResourceState state;
  CommandID time;
};

auto GetReads(const Command &command) -> std::vector<void *>;
auto GetWrites(const Command &command) -> std::vector<void *>;

struct VirtualCommandBuffer {
  friend struct FrameGraph;

  auto Append(const VirtualCommandBuffer &buffer) -> Error {
    ERR_ASSERT(buffer.queueFamily == buffer.queueFamily);

    commands.append_range(buffer.commands);

    return {};
  }

  auto Draw(const Args::VkCmdDraw &arguments) -> void;
  auto DrawIndexed(const Args::VkCmdDrawIndexed &arguments) -> void;
  auto DrawIndirect(const Args::VkCmdDrawIndirect &arguments) -> void;
  auto DrawIndexedIndirect(const Args::VkCmdDrawIndexedIndirect &arguments)
      -> void;
  auto Dispatch(const Args::VkCmdDispatch &arguments) -> void;
  auto DispatchIndirect(const Args::VkCmdDispatchIndirect &arguments) -> void;
  auto BlitImage(const Args::VkCmdBlitImage &arguments) -> void;
  auto PushConstants(const Args::VkCmdPushConstants &arguments) -> void;
  auto CopyBuffer(const Args::VkCmdCopyBuffer &arguments) -> void;
  auto CopyImage(const Args::VkCmdCopyImage &arguments) -> void;
  auto CopyBufferToImage(const Args::VkCmdCopyBufferToImage &arguments) -> void;
  auto CopyImageToBuffer(const Args::VkCmdCopyImageToBuffer &arguments) -> void;

  // TODO: Implement this for layout transitions and mipmapping blits only
  // Storing src, dst textures and integrating it into the GetDependencies method.

  // Custom command, workaround for the barrier system working on an image-based granularity, not range based
  auto MipmapTexture(const Args::MipmapTexture &arguments) -> void;
  auto FillBuffer(const Args::VkCmdFillBuffer &arguments) -> void;
  auto BuildAccelerationStructuresKHR(
      const Args::VkCmdBuildAccelerationStructuresKHR &arguments) -> void;
  auto CopyAccelerationStructureKHR(
      const Args::VkCmdCopyAccelerationStructureKHR &arguments) -> void;
  auto ResetQueryPool(const Args::VkCmdResetQueryPool &arguments) -> void;
  auto WriteAccelerationStructuresPropertiesKHR(
      const Args::VkCmdWriteAccelerationStructuresPropertiesKHR &arguments)
      -> void;
  auto BindIndexBuffer(const Args::VkCmdBindIndexBuffer &arguments) -> void;
  auto BindVertexBuffers(const Args::VkCmdBindVertexBuffers &arguments) -> void;
  auto SetVertexInputEXT(const Args::VkCmdSetVertexInputEXT &arguments) -> void;
  auto BindPipeline(const Args::VkCmdBindPipeline &arguments) -> void;
  auto BindDescriptorSets(const Args::VkCmdBindDescriptorSets &arguments)
      -> void;
  auto SetViewport(const Args::VkCmdSetViewport &arguments) -> void;
  auto SetScissor(const Args::VkCmdSetScissor &arguments) -> void;
  auto SetDepthTestEnable(const Args::VkCmdSetDepthTestEnable &arguments)
      -> void;
  auto SetDepthWriteEnable(const Args::VkCmdSetDepthWriteEnable &arguments)
      -> void;
  auto SetDepthCompareOp(const Args::VkCmdSetDepthCompareOp &arguments) -> void;
  auto
  SetColorBlendEquationEXT(const Args::VkCmdSetColorBlendEquationEXT &arguments)
      -> void;
  auto
  SetColorBlendEnableEXT(const Args::VkCmdSetColorBlendEnableEXT &arguments)
      -> void;
  auto SetColorWriteMaskEXT(const Args::VkCmdSetColorWriteMaskEXT &arguments)
      -> void;
  auto SetCullMode(const Args::VkCmdSetCullMode &arguments) -> void;
  auto SetFrontFace(const Args::VkCmdSetFrontFace &arguments) -> void;
  auto ClearAttachments(const Args::VkCmdClearAttachments &arguments) -> void;
  auto
  BeginDebugUtilsLabelEXT(const Args::VkCmdBeginDebugUtilsLabelEXT &arguments)
      -> void;
  auto EndDebugUtilsLabelEXT(const Args::VkCmdEndDebugUtilsLabelEXT &arguments)
      -> void;
  auto
  InsertDebugUtilsLabelEXT(const Args::VkCmdInsertDebugUtilsLabelEXT &arguments)
      -> void;

  [[nodiscard]] auto GetQueueFamily() const -> uint32_t { return queueFamily; }

  auto AddStateUpdate(VkImage image, const ResourceState &newState) -> void;
  auto AddStateUpdate(VkBuffer buffer, const ResourceState &newState) -> void;
  auto AddStateUpdate(VkAccelerationStructureKHR structure,
                      const ResourceState &newState) -> void;

  auto Reset() -> void;

  auto GetStateID() -> uint32_t {
    auto iter = CommandStateManager::StateToIndex.find(currentState);

    if (iter == CommandStateManager::StateToIndex.end()) {
      CommandStateManager::StateToIndex[currentState] =
          CommandStateManager::States.size();
      CommandStateManager::States.emplace_back(currentState);

      return CommandStateManager::States.size() - 1;
    }

    return iter->second;
  }

  auto GetGraphState() -> GraphState & { return currentState; }

protected:
  // NOLINTBEGIN

  uint64_t time;

  auto AddCommand(const Command &command) -> void;

  std::vector<Command> commands;

  // state updates get set as a parent to the next valid command
  std::vector<Command> stateUpdateCalls;

  std::unordered_map<VkBuffer, std::vector<std::pair<ResourceState, uint64_t>>>
      bufferStateUpdates;
  std::vector<BufferStateUpdate> bufferStateUpdateTimeline;

  std::unordered_map<VkImage, std::vector<std::pair<ResourceState, uint64_t>>>
      imageStateUpdates;

  std::vector<ImageStateUpdate> imageStateUpdateTimeline;

  uint32_t queueFamily;

  GraphState currentState;

  // NOLINTEND
};

auto CreateCommandBuffer() -> VirtualCommandBuffer;
auto ResetCommandBuffer(VirtualCommandBuffer &buffer) -> void;

} // namespace Graphics