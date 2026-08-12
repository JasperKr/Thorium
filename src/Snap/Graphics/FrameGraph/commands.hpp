#pragma once

#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Graphics/graphicsState.hpp"
#include "Libraries/vma.hpp"
#include "Modules/error.hpp"
#include "Modules/stackVector.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {

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

  vkCmdPushConstants,

  vkCmdCopyBuffer,
  vkCmdCopyImage,
  vkCmdCopyBufferToImage,
  vkCmdCopyImageToBuffer,

  vkCmdPipelineBarrier2,
  vkCmdFillBuffer,

  vkCmdBuildAccelerationStructuresKHR,
  vkCmdCopyAccelerationStructureKHR,
  vkCmdResetQueryPool,
  vkCmdWriteAccelerationStructuresPropertiesKHR,

  vkCmdBindIndexBuffer,
  vkCmdBindVertexBuffers,
  vkCmdSetVertexInputEXT,
  vkCmdBindPipeline,

  vkCmdBindDescriptorSets,
  vkCmdSetViewport,
  vkCmdSetScissor,
  vkCmdSetDepthTestEnable,
  vkCmdSetDepthWriteEnable,
  vkCmdSetDepthCompareOp,
  vkCmdSetColorBlendEquationEXT,
  vkCmdSetColorBlendEnableEXT,
  vkCmdSetColorWriteMaskEXT,
  vkCmdSetCullMode,
  vkCmdSetFrontFace,

  vkCmdClearAttachments,

  vkCmdBeginDebugUtilsLabelEXT,
  vkCmdEndDebugUtilsLabelEXT,
  vkCmdInsertDebugUtilsLabelEXT,
};

// NOLINTBEGIN(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTBEGIN(hicpp-explicit-conversions)

struct Callable {
  virtual ~Callable() = default;
  virtual auto Call(VkCommandBuffer cmdBuffer) const -> void = 0;
};

struct BoundResources {
  std::vector<VkImageView> boundImages;
  std::vector<VkBuffer> boundBuffers;
  std::vector<VkAccelerationStructureKHR> boundAccelerationStructures;
  std::vector<VkImageView> colorAttachments;
  VkImageView depthStencilAttachment;

  std::vector<void *> bound;
  std::vector<VkBuffer> vertexBuffers;
  VkBuffer indexBuffer = VK_NULL_HANDLE;

  BoundResources();
};

namespace Args {

struct VkCmdDraw : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdDraw;

  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;

  VkCmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance)
      : vertexCount(vertexCount), instanceCount(instanceCount),
        firstVertex(firstVertex), firstInstance(firstInstance) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex,
              firstInstance);
  }
};

struct VkCmdDrawIndexed : Callable, BoundResources {
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex,
                     vertexOffset, firstInstance);
  }
};

struct VkCmdDrawIndirect : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  VkCmdDrawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                    uint32_t stride)
      : buffer(buffer), offset(offset), drawCount(drawCount), stride(stride) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdDrawIndirect(cmdBuffer, buffer, offset, drawCount, stride);
  }
};

struct VkCmdDrawIndexedIndirect : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndexedIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  VkCmdDrawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset,
                           uint32_t drawCount, uint32_t stride)
      : buffer(buffer), offset(offset), drawCount(drawCount), stride(stride) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdDrawIndexedIndirect(cmdBuffer, buffer, offset, drawCount, stride);
  }
};

struct VkCmdDispatch : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdDispatch;

  uint32_t groupCountX;
  uint32_t groupCountY;
  uint32_t groupCountZ;

  VkCmdDispatch(uint32_t groupCountX, uint32_t groupCountY,
                uint32_t groupCountZ)
      : groupCountX(groupCountX), groupCountY(groupCountY),
        groupCountZ(groupCountZ) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);
  }
};

struct VkCmdDispatchIndirect : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdDispatchIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;

  VkCmdDispatchIndirect(VkBuffer buffer, VkDeviceSize offset)
      : buffer(buffer), offset(offset) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdDispatchIndirect(cmdBuffer, buffer, offset);
  }
};

struct VkCmdBlitImage : Callable {
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdBlitImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data(), filter);
  }
};

struct VkCmdPushConstants : Callable {
  static const CommandType type = CommandType::vkCmdPushConstants;

  VkPipelineLayout layout;
  VkShaderStageFlags stageFlags;
  uint32_t offset;
  std::vector<char> values;

  VkCmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                     uint32_t offset, uint32_t size, const void *pValues)
      : layout(layout), stageFlags(stageFlags), offset(offset),
        values(static_cast<const char *>(pValues),
               static_cast<const char *>(pValues) + size) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdPushConstants(cmdBuffer, layout, stageFlags, offset, values.size(),
                       values.data());
  }
};

struct VkCmdCopyBuffer : Callable {
  static const CommandType type = CommandType::vkCmdCopyBuffer;

  VkBuffer srcBuffer;
  VkBuffer dstBuffer;
  std::vector<VkBufferCopy> regions;

  VkCmdCopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                  const VkBufferCopy *pRegions)
      : srcBuffer(srcBuffer), dstBuffer(dstBuffer),
        regions(pRegions, pRegions + regionCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, regions.size(),
                    regions.data());
  }
};

struct VkCmdCopyImage : Callable {
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdCopyImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data());
  }
};

struct VkCmdCopyBufferToImage : Callable {
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage, dstImageLayout,
                           regions.size(), regions.data());
  }
};

struct VkCmdCopyImageToBuffer : Callable {
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, dstBuffer,
                           regions.size(), regions.data());
  }
};

struct VkCmdPipelineBarrier2 : Callable {
  static const CommandType type = CommandType::vkCmdPipelineBarrier2;

  VkDependencyInfo dependencyInfo;

  VkCmdPipelineBarrier2(const VkDependencyInfo *pDependencyInfo)
      : dependencyInfo(*pDependencyInfo) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
  }
};

struct VkCmdFillBuffer : Callable {
  static const CommandType type = CommandType::vkCmdFillBuffer;

  VkBuffer dstBuffer;
  VkDeviceSize dstOffset;
  VkDeviceSize size;
  uint32_t data;

  VkCmdFillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size,
                  uint32_t data)
      : dstBuffer(dstBuffer), dstOffset(dstOffset), size(size), data(data) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdFillBuffer(cmdBuffer, dstBuffer, dstOffset, size, data);
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR *>
        ppBuildRangeInfos;
    ppBuildRangeInfos.reserve(buildRangeInfos.size());
    for (const auto &range : buildRangeInfos) {
      ppBuildRangeInfos.push_back(&range);
    }
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, infoCount, infos.data(),
                                        ppBuildRangeInfos.data());
  }
};

struct VkCmdCopyAccelerationStructureKHR : Callable {
  static const CommandType type =
      CommandType::vkCmdCopyAccelerationStructureKHR;

  VkCopyAccelerationStructureInfoKHR structureInfo;

  VkCmdCopyAccelerationStructureKHR(
      const VkCopyAccelerationStructureInfoKHR *pInfo)
      : structureInfo(*pInfo) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdCopyAccelerationStructureKHR(cmdBuffer, &structureInfo);
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdResetQueryPool(cmdBuffer, queryPool, firstQuery, queryCount);
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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdWriteAccelerationStructuresPropertiesKHR(
        cmdBuffer, accelerationStructures.size(), accelerationStructures.data(),
        queryType, queryPool, firstQuery);
  }
};

struct VkCmdBindIndexBuffer : Callable {
  static const CommandType type = CommandType::vkCmdBindIndexBuffer;

  VkBuffer buffer;
  VkDeviceSize offset;
  VkIndexType indexType;

  VkCmdBindIndexBuffer(VkBuffer buffer, VkDeviceSize offset,
                       VkIndexType indexType)
      : buffer(buffer), offset(offset), indexType(indexType) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);
  }
};

struct VkCmdBindVertexBuffers : Callable {
  static const CommandType type = CommandType::vkCmdBindVertexBuffers;

  uint32_t firstBinding;
  std::vector<VkBuffer> buffers;
  std::vector<VkDeviceSize> offsets;

  VkCmdBindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount,
                         const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
      : firstBinding(firstBinding), buffers(pBuffers, pBuffers + bindingCount),
        offsets(pOffsets, pOffsets + bindingCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdBindVertexBuffers(cmdBuffer, firstBinding, buffers.size(),
                           buffers.data(), offsets.data());
  }
};

struct VkCmdSetVertexInputEXT : Callable {
  static const CommandType type = CommandType::vkCmdSetVertexInputEXT;

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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetVertexInputEXT(
        cmdBuffer, bindingDescriptions.size(), bindingDescriptions.data(),
        attributeDescriptions.size(), attributeDescriptions.data());
  }
};

struct VkCmdBindPipeline : Callable {
  static const CommandType type = CommandType::vkCmdBindPipeline;

  VkPipelineBindPoint pipelineBindPoint;
  VkPipeline pipeline;

  VkCmdBindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
      : pipelineBindPoint(pipelineBindPoint), pipeline(pipeline) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);
  }
};

struct VkCmdBindDescriptorSets : Callable {
  static const CommandType type = CommandType::vkCmdBindDescriptorSets;

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

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet,
                            descriptorSets.size(), descriptorSets.data(),
                            dynamicOffsets.size(), dynamicOffsets.data());
  }
};

struct VkCmdSetViewport : Callable {
  static const CommandType type = CommandType::vkCmdSetViewport;

  uint32_t firstViewport;
  std::vector<VkViewport> viewports;

  VkCmdSetViewport(uint32_t firstViewport, uint32_t viewportCount,
                   const VkViewport *pViewports)
      : firstViewport(firstViewport),
        viewports(pViewports, pViewports + viewportCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetViewport(cmdBuffer, firstViewport, viewports.size(),
                     viewports.data());
  }
};

struct VkCmdSetScissor : Callable {
  static const CommandType type = CommandType::vkCmdSetScissor;

  uint32_t firstScissor;
  std::vector<VkRect2D> scissors;

  VkCmdSetScissor(uint32_t firstScissor, uint32_t scissorCount,
                  const VkRect2D *pScissors)
      : firstScissor(firstScissor),
        scissors(pScissors, pScissors + scissorCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetScissor(cmdBuffer, firstScissor, scissors.size(), scissors.data());
  }
};

struct VkCmdSetDepthTestEnable : Callable {
  static const CommandType type = CommandType::vkCmdSetDepthTestEnable;

  VkBool32 depthTestEnable;

  VkCmdSetDepthTestEnable(VkBool32 depthTestEnable)
      : depthTestEnable(depthTestEnable) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetDepthTestEnable(cmdBuffer, depthTestEnable);
  }
};

struct VkCmdSetDepthWriteEnable : Callable {
  static const CommandType type = CommandType::vkCmdSetDepthWriteEnable;

  VkBool32 depthWriteEnable;

  VkCmdSetDepthWriteEnable(VkBool32 depthWriteEnable)
      : depthWriteEnable(depthWriteEnable) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetDepthWriteEnable(cmdBuffer, depthWriteEnable);
  }
};

struct VkCmdSetDepthCompareOp : Callable {
  static const CommandType type = CommandType::vkCmdSetDepthCompareOp;

  VkCompareOp depthCompareOp;

  VkCmdSetDepthCompareOp(VkCompareOp depthCompareOp)
      : depthCompareOp(depthCompareOp) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetDepthCompareOp(cmdBuffer, depthCompareOp);
  }
};

struct VkCmdSetColorBlendEquationEXT : Callable {
  static const CommandType type = CommandType::vkCmdSetColorBlendEquationEXT;

  uint32_t firstAttachment;
  std::vector<VkColorBlendEquationEXT> equations;

  VkCmdSetColorBlendEquationEXT(
      uint32_t firstAttachment, uint32_t attachmentCount,
      const VkColorBlendEquationEXT *pColorBlendEquations)
      : firstAttachment(firstAttachment),
        equations(pColorBlendEquations,
                  pColorBlendEquations + attachmentCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetColorBlendEquationEXT(cmdBuffer, firstAttachment, equations.size(),
                                  equations.data());
  }
};

struct VkCmdSetColorBlendEnableEXT : Callable {
  static const CommandType type = CommandType::vkCmdSetColorBlendEnableEXT;

  uint32_t firstAttachment;
  std::vector<VkBool32> enables;

  VkCmdSetColorBlendEnableEXT(uint32_t firstAttachment,
                              uint32_t attachmentCount,
                              const VkBool32 *pColorBlendEnables)
      : firstAttachment(firstAttachment),
        enables(pColorBlendEnables, pColorBlendEnables + attachmentCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetColorBlendEnableEXT(cmdBuffer, firstAttachment, enables.size(),
                                enables.data());
  }
};

struct VkCmdSetColorWriteMaskEXT : Callable {
  static const CommandType type = CommandType::vkCmdSetColorWriteMaskEXT;

  uint32_t firstAttachment;
  std::vector<VkColorComponentFlags> masks;

  VkCmdSetColorWriteMaskEXT(uint32_t firstAttachment, uint32_t attachmentCount,
                            const VkColorComponentFlags *pColorWriteMasks)
      : firstAttachment(firstAttachment),
        masks(pColorWriteMasks, pColorWriteMasks + attachmentCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetColorWriteMaskEXT(cmdBuffer, firstAttachment, masks.size(),
                              masks.data());
  }
};

struct VkCmdSetCullMode : Callable {
  static const CommandType type = CommandType::vkCmdSetCullMode;

  VkCullModeFlags cullMode;

  VkCmdSetCullMode(VkCullModeFlags cullMode) : cullMode(cullMode) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetCullMode(cmdBuffer, cullMode);
  }
};

struct VkCmdSetFrontFace : Callable {
  static const CommandType type = CommandType::vkCmdSetFrontFace;

  VkFrontFace frontFace;

  VkCmdSetFrontFace(VkFrontFace frontFace) : frontFace(frontFace) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdSetFrontFace(cmdBuffer, frontFace);
  }
};

struct VkCmdClearAttachments : Callable {
  static const CommandType type = CommandType::vkCmdClearAttachments;

  std::vector<VkClearAttachment> attachments;
  std::vector<VkClearRect> rects;

  VkCmdClearAttachments(uint32_t attachmentCount,
                        const VkClearAttachment *pAttachments,
                        uint32_t rectCount, const VkClearRect *pRects)
      : attachments(pAttachments, pAttachments + attachmentCount),
        rects(pRects, pRects + rectCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdClearAttachments(cmdBuffer, attachments.size(), attachments.data(),
                          rects.size(), rects.data());
  }
};

struct VkCmdBeginDebugUtilsLabelEXT : Callable {
  static const CommandType type = CommandType::vkCmdBeginDebugUtilsLabelEXT;

  VkDebugUtilsLabelEXT labelInfo;

  VkCmdBeginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT *pLabelInfo)
      : labelInfo(*pLabelInfo) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &labelInfo);
  }
};

struct VkCmdEndDebugUtilsLabelEXT : Callable {
  static const CommandType type = CommandType::vkCmdEndDebugUtilsLabelEXT;

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
  }
};

struct VkCmdInsertDebugUtilsLabelEXT : Callable {
  static const CommandType type = CommandType::vkCmdInsertDebugUtilsLabelEXT;

  VkDebugUtilsLabelEXT labelInfo;

  VkCmdInsertDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT *pLabelInfo)
      : labelInfo(*pLabelInfo) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> void override {
    vkCmdInsertDebugUtilsLabelEXT(cmdBuffer, &labelInfo);
  }
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
    Args::VkCmdDispatchIndirect, Args::VkCmdBlitImage, Args::VkCmdPushConstants,
    Args::VkCmdCopyBuffer, Args::VkCmdCopyImage, Args::VkCmdCopyBufferToImage,
    Args::VkCmdCopyImageToBuffer, Args::VkCmdPipelineBarrier2,
    Args::VkCmdFillBuffer, Args::VkCmdBuildAccelerationStructuresKHR,
    Args::VkCmdCopyAccelerationStructureKHR, Args::VkCmdResetQueryPool,
    Args::VkCmdWriteAccelerationStructuresPropertiesKHR,
    Args::VkCmdBindIndexBuffer, Args::VkCmdBindVertexBuffers,
    Args::VkCmdSetVertexInputEXT, Args::VkCmdBindPipeline,
    Args::VkCmdBindDescriptorSets, Args::VkCmdSetViewport,
    Args::VkCmdSetScissor, Args::VkCmdSetDepthTestEnable,
    Args::VkCmdSetDepthWriteEnable, Args::VkCmdSetDepthCompareOp,
    Args::VkCmdSetColorBlendEquationEXT, Args::VkCmdSetColorBlendEnableEXT,
    Args::VkCmdSetColorWriteMaskEXT, Args::VkCmdSetCullMode,
    Args::VkCmdSetFrontFace, Args::VkCmdClearAttachments,
    Args::VkCmdBeginDebugUtilsLabelEXT, Args::VkCmdEndDebugUtilsLabelEXT,
    Args::VkCmdInsertDebugUtilsLabelEXT>;

struct Command {
  uint64_t id = UINT64_MAX;

  ArgVariants data;

  explicit Command(ArgVariants params) : data(std::move(params)) {}

  [[nodiscard]] auto GetType() const -> CommandType {
    return std::visit(
        [](const auto &current) -> CommandType { return current.type; }, data);
  }

  [[nodiscard]] auto GetBoundResources() -> BoundResources * {
    return get_if_derived<BoundResources>(data);
  }

  [[nodiscard]] auto GetBoundResources() const -> BoundResources const * {
    return get_if_derived<BoundResources>(data);
  }
};

struct BufferStateUpdate {
  VkBuffer buffer;
  ResourceState state;
  uint64_t time;
};

struct ImageStateUpdate {
  VkImageView image;
  ResourceState state;
  uint64_t time;
};

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
  auto PipelineBarrier2(const Args::VkCmdPipelineBarrier2 &arguments) -> void;
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

  auto AddStateUpdate(VkImageView image, const ResourceState &newState) -> void;
  auto AddStateUpdate(VkBuffer buffer, const ResourceState &newState) -> void;
  auto AddStateUpdate(VkAccelerationStructureKHR structure,
                      const ResourceState &newState) -> void;

protected:
  // NOLINTBEGIN

  uint64_t time;

  auto AddCommand(const Command &command) -> void;

  std::vector<Command> commands;

  std::unordered_map<VkBuffer, std::vector<std::pair<ResourceState, uint64_t>>>
      bufferStateUpdates;
  std::vector<BufferStateUpdate> bufferStateUpdateTimeline;

  std::unordered_map<VkImageView,
                     std::vector<std::pair<ResourceState, uint64_t>>>
      imageStateUpdates;

  std::vector<ImageStateUpdate> imageStateUpdateTimeline;

  uint32_t queueFamily;

  // NOLINTEND
};

} // namespace Graphics