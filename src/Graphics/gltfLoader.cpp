#pragma once

#include "gltfLoader.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/material.hpp"
#include "Modules/model.hpp"
#include "Modules/object.hpp"
#include <cstdint>
#include <span>
#include <string>

// #define FASTGLTF_USE_STD_MODULE

#include "fastgltf/include/fastgltf/core.hpp"
#include "fastgltf/include/fastgltf/types.hpp"

#include <variant>
#include <vector>

namespace glTF {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static fastgltf::Parser Parser =
    fastgltf::Parser(fastgltf::Extensions::KHR_lights_punctual);

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const fastgltf::DataSource &dataSource)
    -> Result<std::vector<std::byte>>;

inline auto LoadBufferView(const fastgltf::Asset &asset,
                           const fastgltf::BufferView &bufferView)
    -> Result<std::vector<std::byte>> {
  const auto &buffer = asset.buffers[bufferView.bufferIndex];

  auto bufferData = LoadDataSource(asset, buffer.data);
  if (Error::IsError(bufferData)) {
    return bufferData.error().AsUnexpected();
  }

  const auto &bytes = bufferData.value();

  const auto byteOffset = bufferView.byteOffset;
  const auto byteLength = bufferView.byteLength;

  if (byteOffset + byteLength > bytes.size()) {
    return Error::Unexpected("Buffer view out of bounds.");
  }

  // Pointer arithmetic galore
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return std::vector<std::byte>(bytes.data() + byteOffset,
                                bytes.data() + byteOffset + byteLength);
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

inline auto LoadURI(const fastgltf::sources::URI &uriSource)
    -> Result<std::vector<std::byte>> {
  const auto &uri = uriSource.uri;
  const auto &path = std::string(uri.path()); // (TODO: is this correct?)

  auto fileResult = Filesystem::ReadFile(path);

  if (Error::IsError(fileResult)) {
    return fileResult.error().AsUnexpected();
  }

  auto bytes = fileResult.value();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto *bytedata = reinterpret_cast<std::byte *>(bytes.data());

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return std::vector<std::byte>(bytedata, bytedata + bytes.size());
}

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const fastgltf::DataSource &dataSource)
    -> Result<std::vector<std::byte>> {

  // never monostate
  // std::variant<std::monostate, sources::BufferView, sources::URI, sources::Array, sources::Vector, sources::CustomBuffer, sources::ByteView, sources::Fallback>

  if (std::holds_alternative<fastgltf::sources::BufferView>(dataSource)) {
    const auto &view = std::get<fastgltf::sources::BufferView>(dataSource);
    const auto &bufferView = asset.bufferViews[view.bufferViewIndex];
    return LoadBufferView(asset, bufferView);
  }

  if (std::holds_alternative<fastgltf::sources::URI>(dataSource)) {
    const auto &uriSource = std::get<fastgltf::sources::URI>(dataSource);
    return LoadURI(uriSource);
  }

  if (std::holds_alternative<fastgltf::sources::Array>(dataSource)) {
    const auto &arraySource = std::get<fastgltf::sources::Array>(dataSource);

    return std::vector<std::byte>(arraySource.bytes.begin(),
                                  arraySource.bytes.end());
  }
  if (std::holds_alternative<fastgltf::sources::Vector>(dataSource)) {
    const auto &vectorSource = std::get<fastgltf::sources::Vector>(dataSource);

    return std::vector<std::byte>(vectorSource.bytes.begin(),
                                  vectorSource.bytes.end());
  }
  if (std::holds_alternative<fastgltf::sources::CustomBuffer>(dataSource)) {
    const auto &customBufferSource =
        std::get<fastgltf::sources::CustomBuffer>(dataSource);

    return Error::Unexpected("Custom buffers not supported.");
  }
  if (std::holds_alternative<fastgltf::sources::ByteView>(dataSource)) {
    const auto &byteViewSource =
        std::get<fastgltf::sources::ByteView>(dataSource);

    return std::vector<std::byte>(byteViewSource.bytes.begin(),
                                  byteViewSource.bytes.end());
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

  auto data = LoadDataSource(asset, image.data);
  if (Error::IsError(data)) {
    return data.error().AsUnexpected();
  }

  auto byteSpan = std::span<
      uint8_t>( // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<uint8_t *>(data.value().data()), data.value().size());

  auto imageDataResult = Image::ImageData::Create(byteSpan);
  if (Error::IsError(imageDataResult)) {
    return imageDataResult.error().AsUnexpected();
  }

  auto imageData = imageDataResult.value();

  Graphics::Texture::TextureCreationInfo createInfo;
  createInfo.width = imageData->GetWidth();
  createInfo.height = imageData->GetHeight();
  createInfo.format = imageData->GetFormat();
  createInfo.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  createInfo.mipmapCount = 1;

  auto textureResult = Graphics::Texture::Create2D(context, createInfo);

  if (Error::IsError(textureResult)) {
    return textureResult.error().AsUnexpected();
  }

  auto textureRef = textureResult.value();

  auto uploadResult = textureRef->SetPixels(context, *imageData);

  if (Error::IsError(uploadResult)) {
    return uploadResult.AsUnexpected();
  }

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
    auto albedoTextureLoadResult = LoadTexture(
        context, asset, gltfMaterial.pbrData.baseColorTexture.value());

    if (Error::IsError(albedoTextureLoadResult)) {
      return albedoTextureLoadResult.error();
    }

    material.AlbedoTexture = albedoTextureLoadResult.value();
  }

  // TODO: Combine metallic, roughness, ao, reflectance into one texture.
  if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
    auto materialTextureLoadResult = LoadTexture(
        context, asset, gltfMaterial.pbrData.metallicRoughnessTexture.value());

    if (Error::IsError(materialTextureLoadResult)) {
      return materialTextureLoadResult.error();
    }

    material.MaterialTexture = materialTextureLoadResult.value();
  }

  if (gltfMaterial.normalTexture.has_value()) {
    auto normalTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.normalTexture.value());

    if (Error::IsError(normalTextureLoadResult)) {
      return normalTextureLoadResult.error();
    }

    material.NormalTexture = normalTextureLoadResult.value();
  }

  if (gltfMaterial.emissiveTexture.has_value()) {
    auto emissiveTextureLoadResult =
        LoadTexture(context, asset, gltfMaterial.emissiveTexture.value());

    if (Error::IsError(emissiveTextureLoadResult)) {
      return emissiveTextureLoadResult.error();
    }

    material.EmissiveTexture = emissiveTextureLoadResult.value();
  }

  return Error::Success();
}

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

  // loop through nodes
  for (const auto &gltfNode : asset.get().nodes) {
    auto nodeResult = LoadNode(context, asset.get(), gltfNode);
    if (Error::IsError(nodeResult)) {
      return nodeResult.error();
    }

    for (auto &sceneObject : nodeResult.value()) {
      scene.Nodes.emplace_back(std::move(sceneObject));
    }
  }

  return Error::Success();
}

} // namespace glTF