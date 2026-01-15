#pragma once

#include "gltfLoader.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/material.hpp"
#include "Modules/model.hpp"
#include "Modules/object.hpp"
#include <cstdint>
#include <span>
#include <string>

#include "fastgltf/include/fastgltf/core.hpp"
#include "fastgltf/include/fastgltf/types.hpp"
#include "vulkan/vulkan_core.h"

#include <utility>
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
                         Renderer::Material &material) -> Error {
  material.Name = gltfMaterial.name;

  material.CullMode =
      gltfMaterial.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

  material.AlphaCutoff = gltfMaterial.alphaCutoff;
  material.RoughnessFactor = gltfMaterial.pbrData.roughnessFactor;
  material.MetallicFactor = gltfMaterial.pbrData.metallicFactor;
  material.AlbedoFactor = Math::Vec4(gltfMaterial.pbrData.baseColorFactor[0],
                                     gltfMaterial.pbrData.baseColorFactor[1],
                                     gltfMaterial.pbrData.baseColorFactor[2],
                                     gltfMaterial.pbrData.baseColorFactor[3]);

  // The alpha mode is exactly the same enum as our AlphaMode.
  // But for type safety, we do a manual mapping.

  switch (gltfMaterial.alphaMode) {
  case fastgltf::AlphaMode::Opaque:
    material.AlphaModeSetting = Renderer::AlphaMode::Opaque;
    break;
  case fastgltf::AlphaMode::Mask:
    material.AlphaModeSetting = Renderer::AlphaMode::Mask;
    break;
  case fastgltf::AlphaMode::Blend:
    material.AlphaModeSetting = Renderer::AlphaMode::Blend;
    break;
  }

  material.EmissiveFactor =
      Math::Vec3(gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1],
                 gltfMaterial.emissiveFactor[2]);

  if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
    PrintInfo("Loading albedo texture.");
    auto albedoTextureLoadResult = LoadTexture(
        context, asset, gltfMaterial.pbrData.baseColorTexture.value());

    if (Error::IsError(albedoTextureLoadResult)) {
      return albedoTextureLoadResult.error();
    }

    material.AlbedoTexture = albedoTextureLoadResult.value();
  }

  if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
    PrintInfo("Loading metallic-roughness texture.");
    auto metallicRoughnessLoadResult = LoadTexture(
        context, asset, gltfMaterial.pbrData.metallicRoughnessTexture.value());

    if (Error::IsError(metallicRoughnessLoadResult)) {
      return metallicRoughnessLoadResult.error();
    }

    material.MetallicRoughnessTexture = metallicRoughnessLoadResult.value();
  }

  if (gltfMaterial.occlusionTexture.has_value()) {
    PrintInfo("Loading occlusion texture.");
    auto aoTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.occlusionTexture.value());

    if (Error::IsError(aoTextureLoadResult)) {
      return aoTextureLoadResult.error();
    }

    material.AmbientOcclusionTexture = aoTextureLoadResult.value();
  }

  if (gltfMaterial.normalTexture.has_value()) {
    PrintInfo("Loading normal texture.");
    auto normalTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.normalTexture.value());

    if (Error::IsError(normalTextureLoadResult)) {
      return normalTextureLoadResult.error();
    }

    material.NormalTexture = normalTextureLoadResult.value();
  }

  if (gltfMaterial.emissiveTexture.has_value()) {
    PrintInfo("Loading emissive texture.");
    auto emissiveTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.emissiveTexture.value());

    if (Error::IsError(emissiveTextureLoadResult)) {
      return emissiveTextureLoadResult.error();
    }

    material.EmissiveTexture = emissiveTextureLoadResult.value();
  }

  return Error::Success();
}

// NOLINTNEXTLINE
inline auto LoadNode(Graphics::GraphicsContext &context,
                     const fastgltf::Asset &asset,
                     const fastgltf::Node &gltfNode)
    -> Result<std::vector<Engine::SceneObject>> {
  bool isMesh = gltfNode.meshIndex.has_value();
  bool isSkin = gltfNode.skinIndex.has_value();
  bool isCamera = gltfNode.cameraIndex.has_value();
  bool isLight = gltfNode.lightIndex.has_value();

  // Note: checking if children aren't empty might cull some nodes, should check if this is an issue.
  bool isNode = !isMesh && !isSkin && !isCamera && !isLight;

  if (isNode) {
    Engine::Node node;
    // Create a new engine node.
    node.Name = gltfNode.name;

    if (std::holds_alternative<fastgltf::TRS>(gltfNode.transform)) {
      const auto &trs = std::get<fastgltf::TRS>(gltfNode.transform);
      node.Transform.Position = Math::Vec3(
          trs.translation[0], trs.translation[1], trs.translation[2]);
      node.Transform.Rotation = Math::Quaternion(
          trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3]);
      node.Transform.Scale =
          Math::Vec3(trs.scale[0], trs.scale[1], trs.scale[2]);
    } else {
      // Shouldn't be hit. Since we specified DecomposeNodeMatrices, all matrices should
      // have been decomposed.
      return Error::Unexpected(
          "Attempted to load a node with matrix transform.");
    }

    for (const auto &childIndex : gltfNode.children) {
      const auto &childGltfNode = asset.nodes[childIndex];
      auto childNodeResult = LoadNode(context, asset, childGltfNode);
      if (Error::IsError(childNodeResult)) {
        return childNodeResult.error().AsUnexpected();
      }
      // node.Children.emplace_back(childNodeResult.value());

      for (auto &childObject : childNodeResult.value()) {
        node.Children.emplace_back(std::move(childObject));
      }
    }

    return std::vector<Engine::SceneObject>{node};
  }

  if (isMesh) {
    auto meshIndex = *gltfNode.meshIndex;
    const auto &gltfMesh = asset.meshes[meshIndex];

    std::vector<Engine::SceneObject> shapes;

    for (const auto &primitive : gltfMesh.primitives) {
      // Load each primitive into a shape.
      Engine::Shape shape;
      shape.Name = gltfNode.name;

      // Load material if present.
      if (primitive.materialIndex.has_value()) {
        const auto &material = asset.materials[primitive.materialIndex.value()];

        auto materialResult =
            LoadMaterial(context, asset, material, shape.Material);
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
        scene.Nodes.emplace_back(std::move(sceneObject));
      }
    }
  }

  LoadedBuffers.clear();

  return Error::Success();
}

} // namespace glTF