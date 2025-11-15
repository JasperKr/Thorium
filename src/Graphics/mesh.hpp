#pragma once

#include "Modules/error.hpp"
#include "buffer.hpp"
#include "graphics.hpp"
#include "tl/expected.hpp"
#include <iostream>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <unordered_map>

namespace Graphics {

struct VertexFormat {
  std::vector<VkVertexInputAttributeDescription> Attributes;
  std::vector<VkVertexInputBindingDescription> Bindings;
};

struct MeshDrawRange {
  uint32_t Offset, Count;
};

template <typename Vertex> struct Mesh {
  VertexFormat Format = {.Attributes = {}, .Bindings = {}};

  std::vector<Vertex> VertexData;
  std::vector<uint32_t> IndexData;

  Buffer VertexBuffer = {};
  Buffer IndexBuffer = {};

  MeshDrawRange DrawRange = {.Offset = 0, .Count = 0};

  auto VkFormatSize(VkFormat format) -> uint32_t {
    const int floatSize = 4;
    const int intSize = 4;
    const int shortSize = 2;
    const int byteSize = 1;

    switch (format) {
    // Float formats
    case VK_FORMAT_R32_SFLOAT:
      return floatSize;
    case VK_FORMAT_R32G32_SFLOAT:
      return floatSize * 2;
    case VK_FORMAT_R32G32B32_SFLOAT:
      return floatSize * 3;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return floatSize * 4;
    case VK_FORMAT_R16_SFLOAT:
      return shortSize;
    case VK_FORMAT_R16G16_SFLOAT:
      return floatSize;
    case VK_FORMAT_R16G16B16_SFLOAT:
      return shortSize * 3;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return shortSize * 4;

    // Unsigned int formats
    case VK_FORMAT_R8_UINT:
      return byteSize * 1;
    case VK_FORMAT_R8G8_UINT:
      return byteSize * 2;
    case VK_FORMAT_R8G8B8_UINT:
      return byteSize * 3;
    case VK_FORMAT_R8G8B8A8_UINT:
      return byteSize * 4;
    case VK_FORMAT_R16_UINT:
      return shortSize * 1;
    case VK_FORMAT_R16G16_UINT:
      return shortSize * 2;
    case VK_FORMAT_R16G16B16_UINT:
      return shortSize * 3;
    case VK_FORMAT_R16G16B16A16_UINT:
      return shortSize * 4;
    case VK_FORMAT_R32_UINT:
      return intSize;
    case VK_FORMAT_R32G32_UINT:
      return intSize * 2;
    case VK_FORMAT_R32G32B32_UINT:
      return intSize * 3;
    case VK_FORMAT_R32G32B32A32_UINT:
      return intSize * 4;

    // Signed int formats
    case VK_FORMAT_R8_SINT:
      return byteSize * 1;
    case VK_FORMAT_R8G8_SINT:
      return byteSize * 2;
    case VK_FORMAT_R8G8B8_SINT:
      return byteSize * 3;
    case VK_FORMAT_R8G8B8A8_SINT:
      return byteSize * 4;
    case VK_FORMAT_R16_SINT:
      return shortSize * 1;
    case VK_FORMAT_R16G16_SINT:
      return shortSize * 2;
    case VK_FORMAT_R16G16B16_SINT:
      return shortSize * 3;
    case VK_FORMAT_R16G16B16A16_SINT:
      return shortSize * 4;
    case VK_FORMAT_R32_SINT:
      return intSize;
    case VK_FORMAT_R32G32_SINT:
      return intSize * 2;
    case VK_FORMAT_R32G32B32_SINT:
      return intSize * 3;
    case VK_FORMAT_R32G32B32A32_SINT:
      return intSize * 4;

    // Normalized (packed) formats
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
      return byteSize * 1;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
      return byteSize * 2;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
      return byteSize * 3;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
      return byteSize * 4;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
      return shortSize * 1;
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
      return shortSize * 2;
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
      return shortSize * 3;
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
      return shortSize * 4;

    default:
      return 0; // unsupported / unknown
    }
  }

  auto VertexFormatSize(VertexFormat &format) -> uint32_t {
    uint32_t size = 0;
    for (auto &Attribute : format.Attributes) {
      size += VkFormatSize(Attribute.format);
    }
    return size;
  }

  auto UploadVertices(GraphicsContext &context) -> Error::Error {
    uint32_t dataSize = VertexData.size() * VertexFormatSize(Format);

    return VertexBuffer.SetData(context, VertexData.data(), dataSize, 0);
  }

  auto UploadIndices(GraphicsContext &context) -> Error::Error {
    uint64_t dataSize = IndexData.size() * sizeof(uint32_t);

    return IndexBuffer.SetData(context, IndexData.data(), dataSize, 0);
  }

  static auto Create(GraphicsContext &context, VertexFormat &format,
                     std::vector<Vertex> &vertexData,
                     std::vector<uint32_t> *indexData)
      -> tl::expected<Mesh<Vertex>, Error::Error> {

    Mesh mesh = {};

    uint64_t verticesSize = vertexData.size() * mesh.VertexFormatSize(format);

    bool hasIndices = indexData != nullptr;
    uint32_t indexCount =
        hasIndices ? static_cast<uint32_t>(indexData->size()) : 0;

    uint64_t indicesSize = indexCount * sizeof(uint32_t);

    mesh.Format = format;
    mesh.VertexData = vertexData;
    mesh.IndexData = hasIndices ? *indexData : std::vector<uint32_t>{};

    VkMemoryPropertyFlags properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Graphics::BufferCreationInfo vboCreationInfo = {};
    vboCreationInfo.usage =
        static_cast<uint32_t>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
        static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    vboCreationInfo.properties = properties;
    vboCreationInfo.size = verticesSize;

    auto bufferResult = Buffer::Create(context, vboCreationInfo);

    if (Error::IsError(bufferResult)) {
      return tl::unexpected<Error::Error>(bufferResult.error());
    }

    mesh.VertexBuffer = bufferResult.value();

    Graphics::BufferCreationInfo iboCreationInfo = {};
    iboCreationInfo.usage =
        static_cast<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) |
        static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    iboCreationInfo.properties = properties;
    iboCreationInfo.size = indicesSize;

    bufferResult = Buffer::Create(context, iboCreationInfo);

    if (Error::IsError(bufferResult)) {
      return tl::unexpected<Error::Error>(bufferResult.error());
    }

    mesh.IndexBuffer = bufferResult.value();

    mesh.DrawRange.Offset = 0;
    mesh.DrawRange.Count = indexCount > 0 ? indexCount : vertexData.size();

    Error::Error error = mesh.UploadVertices(context);

    if (Error::IsError(error)) {
      return tl::unexpected<Error::Error>(error);
    }
    error = mesh.UploadIndices(context);

    if (Error::IsError(error)) {
      return tl::unexpected<Error::Error>(error);
    }

    return mesh;
  }

  void Destroy(GraphicsContext &context) {
    VertexBuffer.Destroy(context);
    IndexBuffer.Destroy(context);
  }

  auto GetVertexFormat() -> VertexFormat { return Format; }
  auto GetVertexCount() -> uint32_t {
    return static_cast<uint32_t>(VertexData.size());
  }
  auto GetVertexData() -> auto * { return VertexData.data(); }
  auto GetIndexCount() -> uint32_t {
    return static_cast<uint32_t>(IndexData.size());
  }
  auto GetIndexData() -> void * { return IndexData.data(); }
  void SetDrawRange(MeshDrawRange range) {

    uint32_t maxCount = IndexData.size() > 0
                            ? static_cast<uint32_t>(IndexData.size())
                            : static_cast<uint32_t>(VertexData.size());

    assert(range.Offset >= 0 && range.Offset + range.Count <= maxCount);

    DrawRange.Offset = range.Offset;
    DrawRange.Count = range.Count;
  }
  auto GetDrawRange() -> MeshDrawRange { return DrawRange; }

  void Bind(VkCommandBuffer cmdBuffer) {
    std::vector<VkBuffer> vertexBuffers = {VertexBuffer.handle};
    std::vector<VkDeviceSize> offsets = {0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(),
                           offsets.data());
    if (IndexData.size() > 0) {
      vkCmdBindIndexBuffer(cmdBuffer, IndexBuffer.handle, 0,
                           VK_INDEX_TYPE_UINT32);
    }
  }

  void Draw(GraphicsContext &context) {
    RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
    Bind(renderData.commandBuffers[context.frameIndex]);
    MeshDrawRange range = DrawRange;

    if (IndexData.size() > 0) {
      vkCmdDrawIndexed(renderData.commandBuffers[context.frameIndex],
                       range.Count, 1, range.Offset, 0, 0);
    } else {
      vkCmdDraw(renderData.commandBuffers[context.frameIndex], range.Count, 1,
                range.Offset, 0);
    }
  }

  void DrawInstanced(GraphicsContext &context, uint32_t instanceCount) {
    RenderData renderData = GetRenderData(context, GetCurrentThreadIndex());
    Bind(renderData.commandBuffers[context.frameIndex]);

    MeshDrawRange range = DrawRange;

    if (IndexData.size() > 0) {
      vkCmdDrawIndexed(renderData.commandBuffers[context.frameIndex],
                       range.Count, instanceCount, range.Offset, 0, 0);
    } else {
      vkCmdDraw(renderData.commandBuffers[context.frameIndex], range.Count,
                instanceCount, range.Offset, 0);
    }
  }
};

enum class VertexFormats : uint8_t {
  DEFAULT,
  ANIMATED,
  DEFAULT_INSTANCED,
  ANIMATED_INSTANCED,
  GUI,
};

struct VertexFormatsHash {
  auto operator()(VertexFormats format) const noexcept -> size_t {
    return static_cast<size_t>(format);
  }
};

const static std::unordered_map<const VertexFormats, const VertexFormat,
                                VertexFormatsHash>
    PredefinedVertexFormats = {
        {VertexFormats::DEFAULT,
         {
             .Attributes = {{// Vec3 Position
                             .location = 0,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32_SFLOAT,
                             .offset = 0},
                            {// a2b10g10r10 Normal
                             .location = 1,
                             .binding = 0,
                             .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                             .offset = 12},
                            {
                                // a2b10g10r10 Tangent
                                .location = 2,
                                .binding = 0,
                                .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                .offset = 16,
                            },
                            {// 2x half UV
                             .location = 3,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 20},
                            {// 2x half UV 2
                             .location = 4,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 28}},
             .Bindings = {{.binding = 0,
                           .stride = 36,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
         }},
        {VertexFormats::ANIMATED,
         {
             .Attributes = {{// Vec3 Position
                             .location = 0,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32_SFLOAT,
                             .offset = 0},
                            {// a2b10g10r10 Normal
                             .location = 1,
                             .binding = 0,
                             .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                             .offset = 12},
                            {
                                // a2b10g10r10 Tangent
                                .location = 2,
                                .binding = 0,
                                .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                .offset = 16,
                            },
                            {// 2x half UV
                             .location = 3,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 20},
                            {// 2x half UV 2
                             .location = 4,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 28},
                            {// vec4 Bone Weights
                             .location = 5,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                             .offset = 36},
                            {// uvec4 Bone Indices
                             .location = 6,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32A32_UINT,
                             .offset = 52}},
             .Bindings = {{.binding = 0,
                           .stride = 68,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
         }},
        {VertexFormats::DEFAULT_INSTANCED,
         {
             .Attributes =
                 {
                     {// Vec3 Position
                      .location = 0,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32_SFLOAT,
                      .offset = 0},
                     {// a2b10g10r10 Normal
                      .location = 1,
                      .binding = 0,
                      .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                      .offset = 12},
                     {
                         // a2b10g10r10 Tangent
                         .location = 2,
                         .binding = 0,
                         .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                         .offset = 16,
                     },
                     {// 2x half UV
                      .location = 3,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 20},
                     {// 2x half UV 2
                      .location = 4,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 28},
                     {// mat4 Instance Transform (location 5,6,7,8)
                      .location = 5,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 0},
                     {.location = 6,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 16},
                     {.location = 7,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 32},
                     {.location = 8,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 48},
                 },
             .Bindings = {{.binding = 0,
                           .stride = 36,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
                          {.binding = 1,
                           .stride = 64,
                           .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE}},
         }},
        {VertexFormats::ANIMATED_INSTANCED,
         {
             .Attributes =
                 {
                     {// Vec3 Position
                      .location = 0,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32_SFLOAT,
                      .offset = 0},
                     {// a2b10g10r10 Normal
                      .location = 1,
                      .binding = 0,
                      .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                      .offset = 12},
                     {
                         // a2b10g10r10 Tangent
                         .location = 2,
                         .binding = 0,
                         .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                         .offset = 16,
                     },
                     {// 2x half UV
                      .location = 3,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 20},
                     {// 2x half UV 2
                      .location = 4,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 28},
                     {// vec4 Bone Weights
                      .location = 5,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 36},
                     {// uvec4 Bone Indices
                      .location = 6,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32A32_UINT,
                      .offset = 52},
                     {// mat4 Instance Transform (location 7,8,9,10)
                      .location = 7,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 0},
                     {.location = 8,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 16},
                     {.location = 9,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 32},
                     {.location = 10,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 48},
                 },
             .Bindings = {{.binding = 0,
                           .stride = 68,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
                          {.binding = 1,
                           .stride = 64,
                           .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE}},
         }},
        {VertexFormats::GUI,
         {.Attributes = {{// Vec2 Position
                          .location = 0,
                          .binding = 0,
                          .format = VK_FORMAT_R32G32_SFLOAT,
                          .offset = 0},
                         {// Vec2 UV
                          .location = 1,
                          .binding = 0,
                          .format = VK_FORMAT_R32G32_SFLOAT,
                          .offset = 8},
                         {// unorm8 Color
                          .location = 2,
                          .binding = 0,
                          .format = VK_FORMAT_R8G8B8A8_UNORM,
                          .offset = 16}},
          .Bindings = {{.binding = 0,
                        .stride = 24,
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}}}}};

} // namespace Graphics