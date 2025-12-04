#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "buffer.hpp"
#include "graphics.hpp"
#include "rendertarget.hpp"
#include "tl/expected.hpp"
#include <cstdint>
#include <span>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

#include "vertexformat.hpp"

namespace Graphics {

struct MeshDrawRange {
  uint32_t Offset, Count;
};

static const Type type = Type("Mesh");

struct Mesh : Object {
  VertexFormat Format = {.Attributes = {}, .Bindings = {}};

  std::span<uint8_t> VertexData;
  std::vector<uint32_t> IndexData;

  Ref<Buffer> VertexBuffer;
  Ref<Buffer> IndexBuffer;

  MeshDrawRange DrawRange = {.Offset = 0, .Count = 0};

  static auto VkFormatSize(VkFormat format) -> uint32_t {
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

  static auto VertexFormatSize(VertexFormat &format) -> uint32_t {
    uint32_t size = 0;
    for (auto &Attribute : format.Attributes) {
      size += VkFormatSize(Attribute.format);
    }
    return size;
  }

  auto UploadVertices(GraphicsContext &context) -> Error::Error {
    uint32_t dataSize = VertexData.size() * VertexFormatSize(Format);

    return VertexBuffer->SetData(context, VertexData.data(), dataSize, 0);
  }

  auto UploadIndices(GraphicsContext &context) -> Error::Error {
    uint64_t dataSize = IndexData.size() * sizeof(uint32_t);

    return IndexBuffer->SetData(context, IndexData.data(), dataSize, 0);
  }

  static auto Create(GraphicsContext &context, VertexFormats format,
                     std::span<uint8_t> &vertexData,
                     std::vector<uint32_t> *indexData)
      -> tl::expected<Ref<Mesh>, Error::Error> {

    auto meshData = Ref<Mesh>::Make();
    auto *mesh = meshData.get();

    std::cout << meshData.get() << "\n";

    auto vertexFormatIterator = PredefinedVertexFormats.find(format);

    if (vertexFormatIterator == PredefinedVertexFormats.end()) {
      return tl::make_unexpected(Error::Create(
          "Unsupported vertex format specified for mesh creation."));
    }

    VertexFormat vertexFormat = vertexFormatIterator->second;

    uint64_t verticesSize =
        vertexData.size() * Graphics::Mesh::VertexFormatSize(vertexFormat);

    bool hasIndices = indexData != nullptr;
    uint32_t indexCount =
        hasIndices ? static_cast<uint32_t>(indexData->size()) : 0;

    uint64_t indicesSize = indexCount * sizeof(uint32_t);

    mesh->Format = vertexFormat;
    mesh->VertexData = vertexData;
    mesh->IndexData = hasIndices ? *indexData : std::vector<uint32_t>{};

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

    mesh->VertexBuffer = bufferResult.value();

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

    mesh->IndexBuffer = bufferResult.value();

    mesh->DrawRange.Offset = 0;
    mesh->DrawRange.Count = indexCount > 0 ? indexCount : vertexData.size();

    Error::Error error = mesh->UploadVertices(context);

    if (Error::IsError(error)) {
      return tl::unexpected<Error::Error>(error);
    }

    error = mesh->UploadIndices(context);

    if (Error::IsError(error)) {
      return tl::unexpected<Error::Error>(error);
    }

    return meshData;
  }

  void Destroy(GraphicsContext &context) const {
    VertexBuffer->Destroy(context);
    IndexBuffer->Destroy(context);
  }

  auto GetVertexFormat() const -> VertexFormat { return Format; }
  auto GetVertexCount() const -> uint32_t {
    return static_cast<uint32_t>(VertexData.size());
  }
  auto GetVertexData() -> auto * { return VertexData.data(); }
  auto GetIndexCount() const -> uint32_t {
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
  auto GetDrawRange() const -> MeshDrawRange { return DrawRange; }

  void Bind(VkCommandBuffer cmdBuffer) const {
    std::vector<VkBuffer> vertexBuffers = {VertexBuffer->handle};
    std::vector<VkDeviceSize> offsets = {0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(),
                           offsets.data());
    if (IndexData.size() > 0) {
      vkCmdBindIndexBuffer(cmdBuffer, IndexBuffer->handle, 0,
                           VK_INDEX_TYPE_UINT32);
    }
  }

  auto Draw(GraphicsContext &context) const -> Error::Error {
    auto error = RenderTarget::PrepareDraw(context);
    if (Error::IsError(error)) {
      return error;
    }

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

    return Error::Success();
  }

  auto DrawInstanced(GraphicsContext &context, uint32_t instanceCount) const
      -> Error::Error {
    auto error = RenderTarget::PrepareDraw(context);
    if (Error::IsError(error)) {
      return error;
    }

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

    return Error::Success();
  }

  static auto GetType() -> Type { return type; }
};

} // namespace Graphics