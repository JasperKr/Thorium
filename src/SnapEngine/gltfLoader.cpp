#include "gltfLoader.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Scene/node.hpp"
#include "Scene/shape.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "flecs/addons/cpp/entity.hpp"
#include "material.hpp"
#include <cstdint>
#include <span>
#include <string>

#include "fastgltf/include/fastgltf/core.hpp"
#include "fastgltf/include/fastgltf/types.hpp"

#include "vulkan/vulkan_core.h"

#include <variant>
#include <vector>

namespace glTF {

// Instead of heap allocating all loaded data, we store them here.
// And reference them by the index in the LoadedBuffers vector.
// This way we can avoid many large heap allocations.

struct BufferData {
  bool isHoldingView = false;
  bool isHoldingSpan = false;
  std::vector<uint8_t> Data;
  std::span<const uint8_t> SpanData;

  size_t offset = 0;
  size_t length = 0;
  size_t refIdx = 0;

  auto GetData() -> std::span<const uint8_t>;
  [[nodiscard]] auto GetSize() const -> size_t {
    if (isHoldingView) {
      return length;
    }
    if (isHoldingSpan) {
      return SpanData.size();
    }
    return Data.size();
  }

  // NOLINTNEXTLINE easily swapped parameters
  static auto ViewOther(size_t off, size_t len, size_t ref) -> BufferData {
    BufferData buffData{};
    buffData.isHoldingView = true;
    buffData.offset = off;
    buffData.length = len;
    buffData.refIdx = ref;
    return buffData;
  }

  static auto ViewSpan(std::span<const uint8_t> span) -> BufferData {
    BufferData buffData{};
    buffData.isHoldingSpan = true;
    buffData.SpanData = span;
    return buffData;
  }
};

// NOLINTNEXTLINE
static std::vector<BufferData> LoadedBuffers;

auto BufferData::GetData() -> std::span<const uint8_t> {
  if (isHoldingView) {
    auto parentData = LoadedBuffers[refIdx].GetData();
    // NOLINTNEXTLINE
    return {parentData.data() + offset, length};
  }

  if (isHoldingSpan) {
    return SpanData;
  }

  return {Data.data(), Data.size()};
}

using DataIndex = size_t;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static fastgltf::Parser Parser =
    fastgltf::Parser(fastgltf::Extensions::KHR_lights_punctual);

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const fastgltf::DataSource &dataSource)
    -> Result<DataIndex>;

inline auto LoadBufferView(const fastgltf::Asset &asset,
                           const fastgltf::BufferView &bufferView)
    -> Result<DataIndex> {
  const auto &buffer = asset.buffers[bufferView.bufferIndex];

  auto bufferData = LoadDataSource(asset, buffer.data);
  if (Error::IsError(bufferData)) {
    return bufferData;
  }

  const auto byteOffset = static_cast<long>(bufferView.byteOffset);
  const auto byteLength = static_cast<long>(bufferView.byteLength);

  // if (byteOffset + byteLength > bufferData.value()->GetSize()) {
  //   return Error::Unexpected("Buffer view out of bounds.");
  // }

  // return bufferData.value()->View(byteOffset, byteLength);

  // auto &loadedBuffer = LoadedBuffers[bufferView.bufferIndex];
  // std::vector<uint8_t> bufferViewData(loadedBuffer.begin() + byteOffset,
  //                                     loadedBuffer.begin() + byteOffset +
  //                                         byteLength);

  LoadedBuffers.emplace_back(
      BufferData::ViewOther(byteOffset, byteLength, bufferView.bufferIndex));
  return LoadedBuffers.size() - 1;
}

inline auto LoadURI(const fastgltf::sources::URI &uriSource)
    -> Result<DataIndex> {
  const auto &uri = uriSource.uri;
  const auto &path = std::string(uri.path()); // (TODO: is this correct?)

  auto fileResult = Filesystem::ReadFile(path);

  if (Error::IsError(fileResult)) {
    return fileResult.error().AsUnexpected();
  }

  LoadedBuffers.emplace_back(
      BufferData{.isHoldingView = false, .Data = fileResult.value()});

  // return fileResult.value();
  return LoadedBuffers.size() - 1;
}

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const fastgltf::DataSource &dataSource)
    -> Result<DataIndex> {

  // never monostate
  // std::variant<std::monostate, sources::BufferView, sources::URI, sources::Array, sources::Vector, sources::CustomBuffer, sources::ByteView, sources::Fallback>

  if (std::holds_alternative<fastgltf::sources::BufferView>(dataSource)) {
    const auto &view = std::get<fastgltf::sources::BufferView>(dataSource);
    const auto &bufferView = asset.bufferViews[view.bufferViewIndex];
    auto ret = LoadBufferView(asset, bufferView);
    return ret;
  }

  if (std::holds_alternative<fastgltf::sources::URI>(dataSource)) {
    const auto &uriSource = std::get<fastgltf::sources::URI>(dataSource);
    auto ret = LoadURI(uriSource);
    return ret;
  }

  if (std::holds_alternative<fastgltf::sources::Array>(dataSource)) {
    const auto &arraySource = std::get<fastgltf::sources::Array>(dataSource);

    // NOLINTBEGIN
    auto span = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(arraySource.bytes.data()),
        arraySource.bytes.size());

    LoadedBuffers.emplace_back(BufferData::ViewSpan(span));
    // NOLINTEND

    return LoadedBuffers.size() - 1;
  }
  if (std::holds_alternative<fastgltf::sources::Vector>(dataSource)) {
    const auto &vectorSource = std::get<fastgltf::sources::Vector>(dataSource);

    // NOLINTBEGIN
    LoadedBuffers.emplace_back(BufferData{
        .isHoldingView = false,
        .Data = {
            reinterpret_cast<const uint8_t *>(vectorSource.bytes.data()),
            reinterpret_cast<const uint8_t *>(vectorSource.bytes.data() +
                                              vectorSource.bytes.size()),
        }});
    // NOLINTEND

    return LoadedBuffers.size() - 1;
  }
  if (std::holds_alternative<fastgltf::sources::CustomBuffer>(dataSource)) {
    const auto &customBufferSource =
        std::get<fastgltf::sources::CustomBuffer>(dataSource);

    return Error::Unexpected("Custom buffers not supported.");
  }
  if (std::holds_alternative<fastgltf::sources::ByteView>(dataSource)) {
    const auto &byteViewSource =
        std::get<fastgltf::sources::ByteView>(dataSource);

    // NOLINTBEGIN

    LoadedBuffers.emplace_back(BufferData{
        .isHoldingView = false,
        .Data = {
            reinterpret_cast<const uint8_t *>(byteViewSource.bytes.data()),
            reinterpret_cast<const uint8_t *>(byteViewSource.bytes.data() +
                                              byteViewSource.bytes.size()),
        }});
    // NOLINTEND

    return LoadedBuffers.size() - 1;
  }

  return Error::Unexpected("Unsupported data source.");
}

inline auto LoadTexture(Graphics::GraphicsContext &context,
                        const fastgltf::Asset &asset,
                        const fastgltf::TextureInfo &gltfTexture)
    -> Result<Ref<Graphics::Texture::Texture>> {
  const auto &texture = asset.textures[gltfTexture.textureIndex];
  const auto &sampler = texture.samplerIndex.has_value()
                            ? asset.samplers[texture.samplerIndex.value()]
                            : fastgltf::Sampler{};

  if (!texture.imageIndex.has_value()) {
    return Error::Unexpected("Texture has no image index.");
  }

  const auto &image = asset.images[texture.imageIndex.value()];

  auto loadResult = LoadDataSource(asset, image.data);
  if (Error::IsError(loadResult)) {
    return loadResult.error().AsUnexpected();
  }

  auto dataIdx = loadResult.value();

  auto &data = LoadedBuffers[dataIdx];
  auto span = data.GetData();

  // return Ref<Graphics::Texture::Texture>::Make();

  auto textureResult = Graphics::Texture::LoadFromMemory(
      context, span,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

  if (Error::IsError(textureResult)) {
    return textureResult.error().AsUnexpected();
  }

  auto textureRef = textureResult.value();

  return textureRef;
}

inline auto LoadMaterial(Graphics::GraphicsContext &context,
                         const fastgltf::Asset &asset,
                         const fastgltf::Material &gltfMaterial,
                         Engine::Renderer::Material &material) -> Error {
  material.name = gltfMaterial.name;

  material.cullMode =
      gltfMaterial.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

  material.alphaCutoff = gltfMaterial.alphaCutoff;
  material.roughnessFactor = gltfMaterial.pbrData.roughnessFactor;
  material.metallicFactor = gltfMaterial.pbrData.metallicFactor;
  material.albedoFactor = Math::Vec4(gltfMaterial.pbrData.baseColorFactor[0],
                                     gltfMaterial.pbrData.baseColorFactor[1],
                                     gltfMaterial.pbrData.baseColorFactor[2],
                                     gltfMaterial.pbrData.baseColorFactor[3]);

  // The alpha mode is exactly the same enum as our AlphaMode.
  // But for type safety, we do a manual mapping.

  switch (gltfMaterial.alphaMode) {
  case fastgltf::AlphaMode::Opaque:
    material.alphaMode = Engine::Renderer::AlphaMode::Opaque;
    break;
  case fastgltf::AlphaMode::Mask:
    material.alphaMode = Engine::Renderer::AlphaMode::Mask;
    break;
  case fastgltf::AlphaMode::Blend:
    material.alphaMode = Engine::Renderer::AlphaMode::Blend;
    break;
  }

  material.emissiveFactor =
      Math::Vec3(gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1],
                 gltfMaterial.emissiveFactor[2]);

  if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
    auto albedoTextureLoadResult = LoadTexture(
        context, asset, gltfMaterial.pbrData.baseColorTexture.value());

    if (Error::IsError(albedoTextureLoadResult)) {
      return albedoTextureLoadResult.error();
    }

    material.albedoTexture = albedoTextureLoadResult.value();
  }

  if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
    auto metallicRoughnessLoadResult = LoadTexture(
        context, asset, gltfMaterial.pbrData.metallicRoughnessTexture.value());

    if (Error::IsError(metallicRoughnessLoadResult)) {
      return metallicRoughnessLoadResult.error();
    }

    material.metallicRoughnessTexture = metallicRoughnessLoadResult.value();
  }

  if (gltfMaterial.occlusionTexture.has_value()) {
    auto aoTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.occlusionTexture.value());

    if (Error::IsError(aoTextureLoadResult)) {
      return aoTextureLoadResult.error();
    }

    material.ambientOcclusionTexture = aoTextureLoadResult.value();
  }

  if (gltfMaterial.normalTexture.has_value()) {
    auto normalTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.normalTexture.value());

    if (Error::IsError(normalTextureLoadResult)) {
      return normalTextureLoadResult.error();
    }

    material.normalTexture = normalTextureLoadResult.value();
  }

  if (gltfMaterial.emissiveTexture.has_value()) {
    auto emissiveTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.emissiveTexture.value());

    if (Error::IsError(emissiveTextureLoadResult)) {
      return emissiveTextureLoadResult.error();
    }

    material.emissiveTexture = emissiveTextureLoadResult.value();
  }

  return Error::Success();
}

auto GetVkFormat(fastgltf::ComponentType type, int componentCount)
    -> Result<VkFormat> {

  switch (type) {
  case fastgltf::ComponentType::Byte:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R8_SINT;
    case 2:
      return VK_FORMAT_R8G8_SINT;
    case 3:
      return VK_FORMAT_R8G8B8_SINT;
    case 4:
      return VK_FORMAT_R8G8B8A8_SINT;
    default:
      return Error::Unexpected("Unsupported component count for Byte.");
    }
  case fastgltf::ComponentType::UnsignedByte:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R8_UINT;
    case 2:
      return VK_FORMAT_R8G8_UINT;
    case 3:
      return VK_FORMAT_R8G8B8_UINT;
    case 4:
      return VK_FORMAT_R8G8B8A8_UINT;
    default:
      return Error::Unexpected("Unsupported component count for UnsignedByte.");
    }
  case fastgltf::ComponentType::Short:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R16_SINT;
    case 2:
      return VK_FORMAT_R16G16_SINT;
    case 3:
      return VK_FORMAT_R16G16B16_SINT;
    case 4:
      return VK_FORMAT_R16G16B16A16_SINT;
    default:
      return Error::Unexpected("Unsupported component count for Short.");
    }
  case fastgltf::ComponentType::UnsignedShort:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R16_UINT;
    case 2:
      return VK_FORMAT_R16G16_UINT;
    case 3:
      return VK_FORMAT_R16G16B16_UINT;
    case 4:
      return VK_FORMAT_R16G16B16A16_UINT;
    default:
      return Error::Unexpected(
          "Unsupported component count for UnsignedShort.");
    }
  case fastgltf::ComponentType::Int:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R32_SINT;
    case 2:
      return VK_FORMAT_R32G32_SINT;
    case 3:
      return VK_FORMAT_R32G32B32_SINT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SINT;
    default:
      return Error::Unexpected("Unsupported component count for Int.");
    }
  case fastgltf::ComponentType::UnsignedInt:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R32_UINT;
    case 2:
      return VK_FORMAT_R32G32_UINT;
    case 3:
      return VK_FORMAT_R32G32B32_UINT;
    case 4:
      return VK_FORMAT_R32G32B32A32_UINT;
    default:
      return Error::Unexpected("Unsupported component count for UnsignedInt.");
    }
  case fastgltf::ComponentType::Float:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R32_SFLOAT;
    case 2:
      return VK_FORMAT_R32G32_SFLOAT;
    case 3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      return Error::Unexpected("Unsupported component count for Float.");
    }
  case fastgltf::ComponentType::Double:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R64_SFLOAT;
    case 2:
      return VK_FORMAT_R64G64_SFLOAT;
    case 3:
      return VK_FORMAT_R64G64B64_SFLOAT;
    case 4:
      return VK_FORMAT_R64G64B64A64_SFLOAT;
    default:
      return Error::Unexpected("Unsupported component count for Double.");
    }
  case fastgltf::ComponentType::Invalid:
  default:
    return Error::Unexpected("Unsupported or invalid component type.");
  }
}

auto ExtractVertexFormat(const fastgltf::Asset &asset,
                         const fastgltf::Primitive &primitive)
    -> Result<Graphics::VertexFormat> {
  if (primitive.attributes.empty()) {
    return Error::Unexpected("Primitive is missing attributes.");
  }

  std::vector<Graphics::VertexComponent> attributes;

  for (const auto &[semantic, accessorIndex] : primitive.attributes) {
    const fastgltf::Accessor &accessor = asset.accessors.at(accessorIndex);

    auto size = fastgltf::getComponentByteSize(accessor.componentType);
    auto count = fastgltf::getNumComponents(accessor.type);
  }
}

// NOLINTNEXTLINE
inline auto LoadNode(flecs::world world, Graphics::GraphicsContext &context,
                     const fastgltf::Asset &asset,
                     const fastgltf::Node &gltfNode)
    -> Result<std::vector<flecs::entity>> {
  bool isMesh = gltfNode.meshIndex.has_value();
  bool isSkin = gltfNode.skinIndex.has_value();
  bool isCamera = gltfNode.cameraIndex.has_value();
  bool isLight = gltfNode.lightIndex.has_value();

  // Note: checking if children aren't empty might cull some nodes, should check if this is an issue.
  bool isNode = !isMesh && !isSkin && !isCamera && !isLight;

  if (isNode) {
    auto node = flecs::entity(world, gltfNode.name.c_str());
    node.add<Engine::Node>();
    node.add<Engine::Selectable>(
        Engine::Selectable{.name = std::string(gltfNode.name)});
    node.add<Engine::Transform>();

    if (std::holds_alternative<fastgltf::TRS>(gltfNode.transform)) {
      const auto &trs = std::get<fastgltf::TRS>(gltfNode.transform);

      auto &transform = node.get_mut<Engine::Transform>();

      transform.Position = Math::Vec3(trs.translation[0], trs.translation[1],
                                      trs.translation[2]);
      transform.Rotation = Math::Quaternion(trs.rotation[0], trs.rotation[1],
                                            trs.rotation[2], trs.rotation[3]);
      transform.Scale = Math::Vec3(trs.scale[0], trs.scale[1], trs.scale[2]);
    } else {
      // Shouldn't be hit. Since we specified DecomposeNodeMatrices, all matrices should
      // have been decomposed.
      return Error::Unexpected(
          "Attempted to load a node with matrix transform.");
    }

    for (const auto &childIndex : gltfNode.children) {
      const auto &childGltfNode = asset.nodes[childIndex];
      auto childNodeResult = LoadNode(world, context, asset, childGltfNode);
      if (Error::IsError(childNodeResult)) {
        return childNodeResult.error().AsUnexpected();
      }

      for (auto &childObject : childNodeResult.value()) {
        childObject.child_of(node);
      }
    }

    return std::vector<flecs::entity>{node};
  }

  if (isMesh) {
    auto meshIndex = *gltfNode.meshIndex;
    const auto &gltfMesh = asset.meshes[meshIndex];

    std::vector<flecs::entity> meshes;

    for (const auto &primitive : gltfMesh.primitives) {
      // Load each primitive into a shape.
      // Engine::Shape shape;
      // shape.name = gltfNode.name;

      auto mesh =
          Graphics::Mesh::Create(context, format, vertexCount, debugName);

      // Load material if present.
      if (primitive.materialIndex.has_value()) {
        const auto &material = asset.materials[primitive.materialIndex.value()];
        auto &shape = shapeEntity.get_mut<Engine::Shape>();

        auto materialResult =
            LoadMaterial(context, asset, material, *shape.material);
        if (Error::IsError(materialResult)) {
          return materialResult.AsUnexpected();
        }
      }
    }

    return shapes;
  }

  if (isSkin) {
    return {};
  }

  if (isCamera) {
    return {};
  }

  if (isLight) {
    return {};
  }

  return Error::Unexpected("Failed to determine node type.");
}

auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path,
                   Engine::Scene &scene) -> Error {
  /// Load the file into a data buffer.

  auto bytesResult = Filesystem::ReadFile(path);
  if (Error::IsError(bytesResult)) {
    return bytesResult.error();
  }
  auto bytes = bytesResult.value();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto *bytedata = reinterpret_cast<std::byte *>(bytes.data());

  /// Create a glTF data buffer from the loaded file data.

  auto data = fastgltf::GltfDataBuffer::FromBytes(bytedata, bytes.size());
  if (data.error() != fastgltf::Error::None) {
    // The file couldn't be loaded, or the buffer could not be allocated.
    return Error::Create("Failed to load glTF data buffer.");
  }

  /// Parse the glTF asset from the data buffer.

  auto asset = Parser.loadGltf(data.get(), Path::Directory(path),
                               fastgltf::Options::DecomposeNodeMatrices);
  if (auto error = asset.error(); error != fastgltf::Error::None) {
    // Some error occurred while reading the buffer, parsing the JSON, or validating the data.
    return Error::Create("Invalid glTF asset.");
  }

  /// Validate the loaded asset.

  auto validationError = fastgltf::validate(asset.get());

  if (validationError != fastgltf::Error::None) {
    return Error::Create("glTF asset validation failed: " +
                         std::string(fastgltf::getErrorName(validationError)));
  }

  // loop through scenes
  for (const auto &glTFScene : asset->scenes) {
    PrintInfo("Loading glTF scene: {}", glTFScene.name);
    for (const auto &nodeIndex : glTFScene.nodeIndices) {
      const auto &gltfNode = asset->nodes[nodeIndex];
      PrintInfo("Loading glTF node: {}", gltfNode.name);
      auto nodeResult = LoadNode(context, asset.get(), gltfNode);
      if (Error::IsError(nodeResult)) {
        return nodeResult.error();
      }

      for (auto &sceneObject : nodeResult.value()) {
        scene.hierarchy.emplace_back(std::move(sceneObject));
      }
    }
  }

  LoadedBuffers.clear();

  return Error::Success();
}

} // namespace glTF