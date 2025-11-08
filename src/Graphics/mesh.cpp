// #include "mesh.hpp"
// #include "graphics.hpp"

// namespace Graphics {

// auto VkFormatSize(VkFormat format) -> uint32_t {
//   const int floatSize = 4;
//   const int intSize = 4;
//   const int shortSize = 2;
//   const int byteSize = 1;

//   switch (format) {
//   // Float formats
//   case VK_FORMAT_R32_SFLOAT:
//     return floatSize;
//   case VK_FORMAT_R32G32_SFLOAT:
//     return floatSize * 2;
//   case VK_FORMAT_R32G32B32_SFLOAT:
//     return floatSize * 3;
//   case VK_FORMAT_R32G32B32A32_SFLOAT:
//     return floatSize * 4;
//   case VK_FORMAT_R16_SFLOAT:
//     return shortSize;
//   case VK_FORMAT_R16G16_SFLOAT:
//     return floatSize;
//   case VK_FORMAT_R16G16B16_SFLOAT:
//     return shortSize * 3;
//   case VK_FORMAT_R16G16B16A16_SFLOAT:
//     return shortSize * 4;

//   // Unsigned int formats
//   case VK_FORMAT_R8_UINT:
//     return byteSize * 1;
//   case VK_FORMAT_R8G8_UINT:
//     return byteSize * 2;
//   case VK_FORMAT_R8G8B8_UINT:
//     return byteSize * 3;
//   case VK_FORMAT_R8G8B8A8_UINT:
//     return byteSize * 4;
//   case VK_FORMAT_R16_UINT:
//     return shortSize * 1;
//   case VK_FORMAT_R16G16_UINT:
//     return shortSize * 2;
//   case VK_FORMAT_R16G16B16_UINT:
//     return shortSize * 3;
//   case VK_FORMAT_R16G16B16A16_UINT:
//     return shortSize * 4;
//   case VK_FORMAT_R32_UINT:
//     return intSize;
//   case VK_FORMAT_R32G32_UINT:
//     return intSize * 2;
//   case VK_FORMAT_R32G32B32_UINT:
//     return intSize * 3;
//   case VK_FORMAT_R32G32B32A32_UINT:
//     return intSize * 4;

//   // Signed int formats
//   case VK_FORMAT_R8_SINT:
//     return byteSize * 1;
//   case VK_FORMAT_R8G8_SINT:
//     return byteSize * 2;
//   case VK_FORMAT_R8G8B8_SINT:
//     return byteSize * 3;
//   case VK_FORMAT_R8G8B8A8_SINT:
//     return byteSize * 4;
//   case VK_FORMAT_R16_SINT:
//     return shortSize * 1;
//   case VK_FORMAT_R16G16_SINT:
//     return shortSize * 2;
//   case VK_FORMAT_R16G16B16_SINT:
//     return shortSize * 3;
//   case VK_FORMAT_R16G16B16A16_SINT:
//     return shortSize * 4;
//   case VK_FORMAT_R32_SINT:
//     return intSize;
//   case VK_FORMAT_R32G32_SINT:
//     return intSize * 2;
//   case VK_FORMAT_R32G32B32_SINT:
//     return intSize * 3;
//   case VK_FORMAT_R32G32B32A32_SINT:
//     return intSize * 4;

//   // Normalized (packed) formats
//   case VK_FORMAT_R8_UNORM:
//   case VK_FORMAT_R8_SNORM:
//     return byteSize * 1;
//   case VK_FORMAT_R8G8_UNORM:
//   case VK_FORMAT_R8G8_SNORM:
//     return byteSize * 2;
//   case VK_FORMAT_R8G8B8_UNORM:
//   case VK_FORMAT_R8G8B8_SNORM:
//     return byteSize * 3;
//   case VK_FORMAT_R8G8B8A8_UNORM:
//   case VK_FORMAT_R8G8B8A8_SNORM:
//     return byteSize * 4;
//   case VK_FORMAT_R16_UNORM:
//   case VK_FORMAT_R16_SNORM:
//     return shortSize * 1;
//   case VK_FORMAT_R16G16_UNORM:
//   case VK_FORMAT_R16G16_SNORM:
//     return shortSize * 2;
//   case VK_FORMAT_R16G16B16_UNORM:
//   case VK_FORMAT_R16G16B16_SNORM:
//     return shortSize * 3;
//   case VK_FORMAT_R16G16B16A16_UNORM:
//   case VK_FORMAT_R16G16B16A16_SNORM:
//     return shortSize * 4;

//   default:
//     return 0; // unsupported / unknown
//   }
// }

// template class Mesh<typename Vertex>;

// template <typename Vertex>
// auto Mesh<Vertex>::VertexFormatSize(VertexFormat &format) -> uint32_t {
//   uint32_t size = 0;
//   for (uint32_t i = 0; i < format.AttributeCount; i++) {
//     size += VkFormatSize(format.Attributes.at(i).format);
//   }
//   return size;
// }

// template <typename Vertex>
// auto Mesh<Vertex>::UploadVertices(GraphicsContext &context) -> Error::Error {
//   uint32_t dataSize = VertexData.size() * VertexFormatSize(&Format);

//   return Buffer_SetData(context, &VertexBuffer, VertexData, dataSize, 0);
// }

// template <typename Vertex>
// auto Mesh<Vertex>::UploadIndices(GraphicsContext &context) -> Error::Error {
//   uint64_t dataSize = IndexData.size() * sizeof(uint32_t);

//   return Buffer_SetData(context, &IndexBuffer, IndexData, dataSize, 0);
// }

// template <typename Vertex>
// auto Mesh<Vertex>::Create(GraphicsContext &context, VertexFormat &format,
//                           std::vector<Vertex> &vertexData,
//                           std::vector<uint32_t> *indexData)
//     -> tl::expected<Mesh<Vertex>, Error::Error> {

//   Mesh mesh = {};

//   uint64_t verticesSize = vertexData.size() * mesh.VertexFormatSize(format);

//   bool hasIndices = indexData != nullptr;
//   uint32_t indexCount =
//       hasIndices ? static_cast<uint32_t>(indexData->size()) : 0;

//   uint64_t indicesSize = indexCount * sizeof(uint32_t);

//   mesh.Format = format;
//   mesh.VertexData = vertexData;
//   mesh.IndexData = hasIndices ? *indexData : std::vector<uint32_t>{};

//   VkMemoryPropertyFlags properties =
//       static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
//       static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
//       static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

//   Graphics::BufferCreationInfo vboCreationInfo = {};
//   vboCreationInfo.usage =
//       static_cast<uint32_t>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
//       static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
//   ;
//   vboCreationInfo.properties = properties;
//   vboCreationInfo.size = verticesSize;

//   auto bufferResult = Buffer::Create(context, vboCreationInfo);

//   if (Error::IsError(bufferResult)) {
//     return tl::unexpected(bufferResult.error());
//   }

//   mesh.VertexBuffer = bufferResult.value();

//   Graphics::BufferCreationInfo iboCreationInfo = {};
//   iboCreationInfo.usage =
//       static_cast<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) |
//       static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
//   iboCreationInfo.properties = properties;
//   iboCreationInfo.size = indicesSize;

//   bufferResult = Buffer::Create(context, iboCreationInfo);

//   if (Error::IsError(bufferResult)) {
//     return tl::unexpected(bufferResult.error());
//   }

//   mesh.IndexBuffer = bufferResult.value();

//   mesh.DrawRange.Offset = 0;
//   mesh.DrawRange.Count = indexCount > 0 ? indexCount : vertexData.size();

//   Error::Error error = mesh.UploadVertices(context, mesh);

//   if (Error::IsError(error)) {
//     return tl::unexpected(error);
//   }
//   error = mesh.UploadIndices(context, mesh);

//   if (Error::IsError(error)) {
//     return tl::unexpected(error);
//   }

//   return mesh;
// }

// template <typename Vertex>
// auto Mesh<Vertex>::GetVertexFormat() -> VertexFormat {
//   return Format;
// }
// template <typename Vertex> auto Mesh<Vertex>::GetVertexCount() -> uint32_t {
//   return static_cast<uint32_t>(VertexData.size());
// }
// template <typename Vertex> auto Mesh<Vertex>::GetVertexData() -> void * {
//   return VertexData.data();
// }
// template <typename Vertex> auto Mesh<Vertex>::GetIndexCount() -> uint32_t {
//   return static_cast<uint32_t>(IndexData.size());
// }
// template <typename Vertex> auto Mesh<Vertex>::GetIndexData() -> void * {
//   return IndexData.data();
// }
// template <typename Vertex>
// void Mesh<Vertex>::SetDrawRange(MeshDrawRange range) {

//   uint32_t maxCount = IndexData.size() > 0
//                           ? static_cast<uint32_t>(IndexData.size())
//                           : static_cast<uint32_t>(VertexData.size());

//   assert(range.Offset >= 0 && range.Offset + range.Count <= maxCount);

//   DrawRange.Offset = range.Offset;
//   DrawRange.Count = range.Count;
// }
// template <typename Vertex> auto Mesh<Vertex>::GetDrawRange() -> MeshDrawRange
// {
//   return DrawRange;
// }

// template <typename Vertex> void Mesh<Vertex>::Bind(VkCommandBuffer cmdBuffer)
// {
//   std::vector<VkBuffer> vertexBuffers = {VertexBuffer.Handle};
//   std::vector<VkDeviceSize> offsets = {0};
//   vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(),
//   offsets.data()); if (IndexData.size() > 0) {
//     vkCmdBindIndexBuffer(cmdBuffer, IndexBuffer.Handle, 0,
//                          VK_INDEX_TYPE_UINT32);
//   }
// }

// template <typename Vertex> void Mesh<Vertex>::Draw(GraphicsContext &context)
// {
//   RenderData *renderData = GetRenderData(context, GetCurrentThreadIndex());
//   BindMesh(this, renderData->commandBuffers[context.frameIndex]);
//   MeshDrawRange range = DrawRange;

//   if (IndexData.size() > 0) {
//     vkCmdDrawIndexed(renderData->commandBuffers[context.frameIndex],
//                      range.Count, 1, range.Offset, 0, 0);
//   } else {
//     vkCmdDraw(renderData->commandBuffers[context.frameIndex], range.Count, 1,
//               range.Offset, 0);
//   }
// }

// template <typename Vertex>
// void Mesh<Vertex>::DrawInstanced(GraphicsContext &context,
//                                  uint32_t instanceCount) {
//   RenderData *renderData = GetRenderData(context, GetCurrentThreadIndex());
//   BindMesh(this, renderData->commandBuffers[context.frameIndex]);

//   MeshDrawRange range = DrawRange;

//   if (IndexData.size() > 0) {
//     vkCmdDrawIndexed(renderData->commandBuffers[context.frameIndex],
//                      range.Count, instanceCount, range.Offset, 0, 0);
//   } else {
//     vkCmdDraw(renderData->commandBuffers[context.frameIndex], range.Count,
//               instanceCount, range.Offset, 0);
//   }
// }

// } // namespace Graphics