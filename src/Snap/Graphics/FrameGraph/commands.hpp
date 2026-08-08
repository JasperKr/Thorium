#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
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
  vkCmdResetQueryPool,
  vkCmdWriteAccelerationStructuresPropertiesKHR,

  vkCmdBindIndexBuffer,
  vkCmdBindVertexBuffers,
  vkCmdSetVertexInputEXT,
  vkCmdBindPipeline,

  vkCmdBeginRendering,
  vkCmdEndRendering,

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

namespace Args {
// NOLINTBEGIN(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)

struct ArgBase {
  ArgBase() = default;
  ArgBase(const ArgBase &) = default;
  ArgBase(ArgBase &&) = default;
  ~ArgBase() = default;
  auto operator=(const ArgBase &) -> ArgBase & = delete;
  auto operator=(ArgBase &&) -> ArgBase & = delete;
};

struct VkCmdDraw : ArgBase {
  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;
};

struct VkCmdDrawIndexed : ArgBase {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;
};

struct VkCmdDrawIndirect : ArgBase {
  VkBuffer buffer;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;
};

struct VkCmdDrawIndexedIndirect : ArgBase {
  VkBuffer buffer;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;
};

struct VkCmdDispatch : ArgBase {
  uint32_t groupCountX;
  uint32_t groupCountY;
  uint32_t groupCountZ;
};

struct VkCmdDispatchIndirect : ArgBase {
  VkBuffer buffer;
  VkDeviceSize offset;
};

struct VkCmdBlitImage : ArgBase {
  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  uint32_t regionCount;
  const VkImageBlit *pRegions;
  VkFilter filter;

  VkCmdBlitImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageBlit *pRegions,
                 VkFilter filter)
      : srcImage(srcImage), srcImageLayout(srcImageLayout), dstImage(dstImage),
        dstImageLayout(dstImageLayout), regionCount(regionCount),
        filter(filter) {
    VkImageBlit *regions = new VkImageBlit[regionCount]; // NOLINT
    memcpy(regions, pRegions, sizeof(VkImageBlit) * regionCount);
    this->pRegions = regions;
  }

  ~VkCmdBlitImage() { delete[] pRegions; }
};

struct VkCmdPushConstants : ArgBase {
  VkPipelineLayout layout;
  VkShaderStageFlags stageFlags;
  uint32_t offset;
  uint32_t size;
  void *pValues;

  VkCmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                     uint32_t offset, uint32_t size, const void *pValues)
      : layout(layout), stageFlags(stageFlags), offset(offset), size(size) {
    void *values = new char[size]; // NOLINT
    memcpy(values, pValues, size);
    this->pValues = values;
  }

  ~VkCmdPushConstants() { delete[] static_cast<char *>(pValues); }
};

struct VkCmdCopyBuffer : ArgBase {
  VkBuffer srcBuffer;
  VkBuffer dstBuffer;
  uint32_t regionCount;
  const VkBufferCopy *pRegions;

  VkCmdCopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                  const VkBufferCopy *pRegions)
      : srcBuffer(srcBuffer), dstBuffer(dstBuffer), regionCount(regionCount) {
    VkBufferCopy *regions = new VkBufferCopy[regionCount]; // NOLINT
    memcpy(regions, pRegions, sizeof(VkBufferCopy) * regionCount);
    this->pRegions = regions;
  }

  ~VkCmdCopyBuffer() { delete[] pRegions; }
};

struct VkCmdCopyImage : ArgBase {
  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  uint32_t regionCount;
  const VkImageCopy *pRegions;

  VkCmdCopyImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageCopy *pRegions)
      : srcImage(srcImage), srcImageLayout(srcImageLayout), dstImage(dstImage),
        dstImageLayout(dstImageLayout), regionCount(regionCount) {
    VkImageCopy *regions = new VkImageCopy[regionCount]; // NOLINT
    memcpy(regions, pRegions, sizeof(VkImageCopy) * regionCount);
    this->pRegions = regions;
  }

  ~VkCmdCopyImage() { delete[] pRegions; }
};

struct VkCmdCopyBufferToImage : ArgBase {
  VkBuffer srcBuffer;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  uint32_t regionCount;
  const VkBufferImageCopy *pRegions;

  VkCmdCopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage,
                         VkImageLayout dstImageLayout, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions)
      : srcBuffer(srcBuffer), dstImage(dstImage),
        dstImageLayout(dstImageLayout), regionCount(regionCount) {
    VkBufferImageCopy *regions = new VkBufferImageCopy[regionCount]; // NOLINT
    memcpy(regions, pRegions, sizeof(VkBufferImageCopy) * regionCount);
    this->pRegions = regions;
  }

  ~VkCmdCopyBufferToImage() { delete[] pRegions; }
};

struct VkCmdCopyImageToBuffer : ArgBase {
  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkBuffer dstBuffer;
  uint32_t regionCount;
  const VkBufferImageCopy *pRegions;

  VkCmdCopyImageToBuffer(VkImage srcImage, VkImageLayout srcImageLayout,
                         VkBuffer dstBuffer, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions)
      : srcImage(srcImage), srcImageLayout(srcImageLayout),
        dstBuffer(dstBuffer), regionCount(regionCount) {
    VkBufferImageCopy *regions = new VkBufferImageCopy[regionCount]; // NOLINT
    memcpy(regions, pRegions, sizeof(VkBufferImageCopy) * regionCount);
    this->pRegions = regions;
  }

  ~VkCmdCopyImageToBuffer() { delete[] pRegions; }
};

struct VkCmdPipelineBarrier2 : ArgBase {
  const VkDependencyInfo *pDependencyInfo;

  explicit VkCmdPipelineBarrier2(const VkDependencyInfo *pDependencyInfo) {
    VkDependencyInfo *info = new VkDependencyInfo; // NOLINT
    memcpy(info, pDependencyInfo, sizeof(VkDependencyInfo));
    this->pDependencyInfo = info;
  }

  ~VkCmdPipelineBarrier2() { delete pDependencyInfo; }
};

struct VkCmdFillBuffer : ArgBase {
  VkBuffer dstBuffer;
  VkDeviceSize dstOffset;
  VkDeviceSize size;
  uint32_t data;
};

struct VkCmdBuildAccelerationStructuresKHR : ArgBase {
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
};

struct VkCmdResetQueryPool : ArgBase {
  VkQueryPool queryPool;
  uint32_t firstQuery;
  uint32_t queryCount;
};

struct VkCmdWriteAccelerationStructuresPropertiesKHR : ArgBase {
  uint32_t accelerationStructureCount;
  const VkAccelerationStructureKHR *pAccelerationStructures;
  VkQueryType queryType;
  VkQueryPool queryPool;
  uint32_t firstQuery;

  VkCmdWriteAccelerationStructuresPropertiesKHR(
      uint32_t accelerationStructureCount,
      const VkAccelerationStructureKHR *pAccelerationStructures,
      VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
      : accelerationStructureCount(accelerationStructureCount),
        queryType(queryType), queryPool(queryPool), firstQuery(firstQuery) {
    VkAccelerationStructureKHR *structures = // NOLINT
        new VkAccelerationStructureKHR[accelerationStructureCount];
    memcpy((void *)structures,
           static_cast<const void *>(pAccelerationStructures),
           sizeof(VkAccelerationStructureKHR) * accelerationStructureCount);
    this->pAccelerationStructures = structures;
  }

  ~VkCmdWriteAccelerationStructuresPropertiesKHR() {
    delete[] pAccelerationStructures;
  }
};

struct VkCmdBindIndexBuffer : ArgBase {
  VkBuffer buffer;
  VkDeviceSize offset;
  VkIndexType indexType;
};

struct VkCmdBindVertexBuffers : ArgBase {
  uint32_t firstBinding;
  uint32_t bindingCount;
  const VkBuffer *pBuffers;
  const VkDeviceSize *pOffsets;

  VkCmdBindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount,
                         const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
      : firstBinding(firstBinding), bindingCount(bindingCount) {
    VkBuffer *buffers = new VkBuffer[bindingCount]; // NOLINT
    memcpy((void *)buffers, static_cast<const void *>(pBuffers),
           sizeof(VkBuffer) * bindingCount);
    this->pBuffers = buffers;

    VkDeviceSize *offsets = new VkDeviceSize[bindingCount]; // NOLINT
    memcpy(offsets, pOffsets, sizeof(VkDeviceSize) * bindingCount);
    this->pOffsets = offsets;
  }

  ~VkCmdBindVertexBuffers() {
    delete[] pBuffers;
    delete[] pOffsets;
  }
};

struct VkCmdSetVertexInputEXT : ArgBase {
  uint32_t vertexBindingDescriptionCount;
  const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions;
  uint32_t vertexAttributeDescriptionCount;
  const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions;

  VkCmdSetVertexInputEXT(
      uint32_t vertexBindingDescriptionCount,
      const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
      uint32_t vertexAttributeDescriptionCount,
      const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
      : vertexBindingDescriptionCount(vertexBindingDescriptionCount),
        vertexAttributeDescriptionCount(vertexAttributeDescriptionCount) {
    VkVertexInputBindingDescription2EXT *bindingDescs = // NOLINT
        new VkVertexInputBindingDescription2EXT[vertexBindingDescriptionCount];
    memcpy(bindingDescs, pVertexBindingDescriptions,
           sizeof(VkVertexInputBindingDescription2EXT) *
               vertexBindingDescriptionCount);
    this->pVertexBindingDescriptions = bindingDescs;

    VkVertexInputAttributeDescription2EXT *attrDescs = // NOLINT
        new VkVertexInputAttributeDescription2EXT
            [vertexAttributeDescriptionCount];
    memcpy(attrDescs, pVertexAttributeDescriptions,
           sizeof(VkVertexInputAttributeDescription2EXT) *
               vertexAttributeDescriptionCount);
    this->pVertexAttributeDescriptions = attrDescs;
  }

  ~VkCmdSetVertexInputEXT() {
    delete[] pVertexBindingDescriptions;
    delete[] pVertexAttributeDescriptions;
  }
};

struct VkCmdBindPipeline : ArgBase {
  VkPipelineBindPoint pipelineBindPoint;
  VkPipeline pipeline;
};

struct VkCmdBeginRendering : ArgBase {
  const VkRenderingInfo *pRenderingInfo;
};

struct VkCmdEndRendering : ArgBase {};

struct VkCmdBindDescriptorSets : ArgBase {
  VkPipelineBindPoint pipelineBindPoint;
  VkPipelineLayout layout;
  uint32_t firstSet;
  uint32_t descriptorSetCount;
  const VkDescriptorSet *pDescriptorSets;
  uint32_t dynamicOffsetCount;
  const uint32_t *pDynamicOffsets;

  VkCmdBindDescriptorSets(VkPipelineBindPoint pipelineBindPoint,
                          VkPipelineLayout layout, uint32_t firstSet,
                          uint32_t descriptorSetCount,
                          const VkDescriptorSet *pDescriptorSets,
                          uint32_t dynamicOffsetCount,
                          const uint32_t *pDynamicOffsets)
      : pipelineBindPoint(pipelineBindPoint), layout(layout),
        firstSet(firstSet), descriptorSetCount(descriptorSetCount),
        dynamicOffsetCount(dynamicOffsetCount) {
    VkDescriptorSet *sets = new VkDescriptorSet[descriptorSetCount]; // NOLINT
    memcpy(static_cast<void *>(sets),
           static_cast<const void *>(pDescriptorSets),
           sizeof(VkDescriptorSet) * descriptorSetCount);
    this->pDescriptorSets = sets;

    uint32_t *offsets = new uint32_t[dynamicOffsetCount]; // NOLINT
    memcpy(offsets, pDynamicOffsets, sizeof(uint32_t) * dynamicOffsetCount);
    this->pDynamicOffsets = offsets;
  }

  ~VkCmdBindDescriptorSets() {
    delete[] pDescriptorSets;
    delete[] pDynamicOffsets;
  }
};

struct VkCmdSetViewport : ArgBase {
  uint32_t firstViewport;
  uint32_t viewportCount;
  const VkViewport *pViewports;

  VkCmdSetViewport(uint32_t firstViewport, uint32_t viewportCount,
                   const VkViewport *pViewports)
      : firstViewport(firstViewport), viewportCount(viewportCount) {
    VkViewport *viewports = new VkViewport[viewportCount]; // NOLINT
    memcpy(viewports, pViewports, sizeof(VkViewport) * viewportCount);
    this->pViewports = viewports;
  }

  ~VkCmdSetViewport() { delete[] pViewports; }
};

struct VkCmdSetScissor : ArgBase {
  uint32_t firstScissor;
  uint32_t scissorCount;
  const VkRect2D *pScissors;

  VkCmdSetScissor(uint32_t firstScissor, uint32_t scissorCount,
                  const VkRect2D *pScissors)
      : firstScissor(firstScissor), scissorCount(scissorCount) {
    VkRect2D *scissors = new VkRect2D[scissorCount]; // NOLINT
    memcpy(scissors, pScissors, sizeof(VkRect2D) * scissorCount);
    this->pScissors = scissors;
  }

  ~VkCmdSetScissor() { delete[] pScissors; }
};

struct VkCmdSetDepthTestEnable : ArgBase {
  VkBool32 depthTestEnable;
};

struct VkCmdSetDepthWriteEnable : ArgBase {
  VkBool32 depthWriteEnable;
};

struct VkCmdSetDepthCompareOp : ArgBase {
  VkCompareOp depthCompareOp;
};

struct VkCmdSetColorBlendEquationEXT : ArgBase {
  uint32_t firstAttachment;
  uint32_t attachmentCount;
  const VkColorBlendEquationEXT *pColorBlendEquations;

  VkCmdSetColorBlendEquationEXT(
      uint32_t firstAttachment, uint32_t attachmentCount,
      const VkColorBlendEquationEXT *pColorBlendEquations)
      : firstAttachment(firstAttachment), attachmentCount(attachmentCount) {
    VkColorBlendEquationEXT *equations = // NOLINT
        new VkColorBlendEquationEXT[attachmentCount];
    memcpy(equations, pColorBlendEquations,
           sizeof(VkColorBlendEquationEXT) * attachmentCount);
    this->pColorBlendEquations = equations;
  }

  ~VkCmdSetColorBlendEquationEXT() { delete[] pColorBlendEquations; }
};

struct VkCmdSetColorBlendEnableEXT : ArgBase {
  uint32_t firstAttachment;
  uint32_t attachmentCount;
  const VkBool32 *pColorBlendEnables;

  VkCmdSetColorBlendEnableEXT(uint32_t firstAttachment,
                              uint32_t attachmentCount,
                              const VkBool32 *pColorBlendEnables)
      : firstAttachment(firstAttachment), attachmentCount(attachmentCount) {
    VkBool32 *enables = new VkBool32[attachmentCount]; // NOLINT
    memcpy(enables, pColorBlendEnables, sizeof(VkBool32) * attachmentCount);
    this->pColorBlendEnables = enables;
  }

  ~VkCmdSetColorBlendEnableEXT() { delete[] pColorBlendEnables; }
};

struct VkCmdSetColorWriteMaskEXT : ArgBase {
  uint32_t firstAttachment;
  uint32_t attachmentCount;
  const VkColorComponentFlags *pColorWriteMasks;

  VkCmdSetColorWriteMaskEXT(uint32_t firstAttachment, uint32_t attachmentCount,
                            const VkColorComponentFlags *pColorWriteMasks)
      : firstAttachment(firstAttachment), attachmentCount(attachmentCount) {
    VkColorComponentFlags *masks = // NOLINT
        new VkColorComponentFlags[attachmentCount];
    memcpy(masks, pColorWriteMasks,
           sizeof(VkColorComponentFlags) * attachmentCount);
    this->pColorWriteMasks = masks;
  }

  ~VkCmdSetColorWriteMaskEXT() { delete[] pColorWriteMasks; }
};

struct VkCmdSetCullMode : ArgBase {
  VkCullModeFlags cullMode;
};

struct VkCmdSetFrontFace : ArgBase {
  VkFrontFace frontFace;
};

struct VkCmdClearAttachments : ArgBase {
  uint32_t attachmentCount;
  const VkClearAttachment *pAttachments;
  uint32_t rectCount;
  const VkClearRect *pRects;

  VkCmdClearAttachments(uint32_t attachmentCount,
                        const VkClearAttachment *pAttachments,
                        uint32_t rectCount, const VkClearRect *pRects)
      : attachmentCount(attachmentCount), rectCount(rectCount) {
    VkClearAttachment *attachments = // NOLINT
        new VkClearAttachment[attachmentCount];
    memcpy(attachments, pAttachments,
           sizeof(VkClearAttachment) * attachmentCount);
    this->pAttachments = attachments;

    VkClearRect *rects = new VkClearRect[rectCount]; // NOLINT
    memcpy(rects, pRects, sizeof(VkClearRect) * rectCount);
    this->pRects = rects;
  }

  ~VkCmdClearAttachments() {
    delete[] pAttachments;
    delete[] pRects;
  }
};

struct VkCmdBeginDebugUtilsLabelEXT : ArgBase {
  const VkDebugUtilsLabelEXT *pLabelInfo;

  explicit VkCmdBeginDebugUtilsLabelEXT(
      const VkDebugUtilsLabelEXT *pLabelInfo) {
    VkDebugUtilsLabelEXT *info = new VkDebugUtilsLabelEXT; // NOLINT
    memcpy(info, pLabelInfo, sizeof(VkDebugUtilsLabelEXT));
    this->pLabelInfo = info;
  }

  ~VkCmdBeginDebugUtilsLabelEXT() { delete pLabelInfo; }
};

struct VkCmdEndDebugUtilsLabelEXT : ArgBase {};

struct VkCmdInsertDebugUtilsLabelEXT : ArgBase {
  const VkDebugUtilsLabelEXT *pLabelInfo;

  explicit VkCmdInsertDebugUtilsLabelEXT(
      const VkDebugUtilsLabelEXT *pLabelInfo) {
    VkDebugUtilsLabelEXT *info = new VkDebugUtilsLabelEXT; // NOLINT
    memcpy(info, pLabelInfo, sizeof(VkDebugUtilsLabelEXT));
    this->pLabelInfo = info;
  }

  ~VkCmdInsertDebugUtilsLabelEXT() { delete pLabelInfo; }
};

// NOLINTEND(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
} // namespace Args

struct Command {
  explicit Command(CommandType type) : type(type) {}
  Command(const Command &) = default;
  Command(Command &&) = default;
  auto operator=(const Command &) -> Command & = delete;
  auto operator=(Command &&) -> Command & = delete;
  ~Command() = default;

  CommandType type;

  union commandData {
    Args::VkCmdDraw vkCmdDraw;
    Args::VkCmdDrawIndexed vkCmdDrawIndexed;
    Args::VkCmdDrawIndirect vkCmdDrawIndirect;
    Args::VkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
    Args::VkCmdDispatch vkCmdDispatch;
    Args::VkCmdDispatchIndirect vkCmdDispatchIndirect;
    Args::VkCmdBlitImage vkCmdBlitImage;

    Args::VkCmdPushConstants vkCmdPushConstants;

    Args::VkCmdCopyBuffer vkCmdCopyBuffer;
    Args::VkCmdCopyImage vkCmdCopyImage;
    Args::VkCmdCopyBufferToImage vkCmdCopyBufferToImage;
    Args::VkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;

    Args::VkCmdPipelineBarrier2 vkCmdPipelineBarrier2;
    Args::VkCmdFillBuffer vkCmdFillBuffer;

    Args::VkCmdBuildAccelerationStructuresKHR
        vkCmdBuildAccelerationStructuresKHR;
    Args::VkCmdResetQueryPool vkCmdResetQueryPool;
    Args::VkCmdWriteAccelerationStructuresPropertiesKHR
        vkCmdWriteAccelerationStructuresPropertiesKHR;

    Args::VkCmdBindIndexBuffer vkCmdBindIndexBuffer;
    Args::VkCmdBindVertexBuffers vkCmdBindVertexBuffers;
    Args::VkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
    Args::VkCmdBindPipeline vkCmdBindPipeline;

    Args::VkCmdBeginRendering vkCmdBeginRendering;
    Args::VkCmdEndRendering vkCmdEndRendering;

    Args::VkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    Args::VkCmdSetViewport vkCmdSetViewport;
    Args::VkCmdSetScissor vkCmdSetScissor;
    Args::VkCmdSetDepthTestEnable vkCmdSetDepthTestEnable;
    Args::VkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable;
    Args::VkCmdSetDepthCompareOp vkCmdSetDepthCompareOp;
    Args::VkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT;
    Args::VkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT;
    Args::VkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT;
    Args::VkCmdSetCullMode vkCmdSetCullMode;
    Args::VkCmdSetFrontFace vkCmdSetFrontFace;

    Args::VkCmdClearAttachments vkCmdClearAttachments;

    Args::VkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
    Args::VkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
    Args::VkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
  };
};

} // namespace Graphics