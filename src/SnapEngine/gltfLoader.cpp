#include "gltfLoader.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/compressedImageData.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/Geometry/levelOfDetail.hpp"
#include "Scene/Geometry/shape.hpp"
#include "Scene/node.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "material.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <execution>
#include <flecs.h>
#include <mutex>
#include <numeric>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string>

#include "fastgltf/include/fastgltf/core.hpp"
#include "fastgltf/include/fastgltf/types.hpp"

#include "meshoptimizer.h"
#include "vulkan/vulkan_core.h"

#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace glTF {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<std::vector<std::uint8_t>> Buffers;
std::unordered_map<std::string, std::vector<std::uint8_t>> URICache;
std::mutex URICacheMutex{};
std::unordered_map<std::string, uint16_t> NameDuplicateCount;
std::vector<
    std::variant<Ref<Image::ImageData>, Ref<Image::CompressedImageData>>>
    ImageCache;
std::vector<Ref<Graphics::Texture>> TextureCache;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto GetUniqueName(const std::string &baseName) -> std::string {
  auto countIter = NameDuplicateCount.find(baseName);
  uint16_t count = 0;
  if (countIter != NameDuplicateCount.end()) {
    count = countIter->second + 1;
    countIter->second = count;
  } else {
    NameDuplicateCount[baseName] = 0;
  }

  if (count == 0) {
    return baseName;
  }

  return baseName + "_" + std::to_string(count);
}

inline auto GetUniqueName(const char *baseName) -> std::string {
  return GetUniqueName(std::string(baseName));
}

using DataIndex = size_t;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static fastgltf::Parser Parser =
    fastgltf::Parser(fastgltf::Extensions::KHR_lights_punctual);

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const std::string_view &basePath,
                           const fastgltf::DataSource &dataSource)
    -> Result<std::span<uint8_t>>;

inline auto LoadBufferView(const fastgltf::Asset &asset,
                           const fastgltf::BufferView &bufferView)
    -> Result<std::span<uint8_t>> {
  const auto &buffer = asset.buffers[bufferView.bufferIndex];

  const auto byteOffset = static_cast<long>(bufferView.byteOffset);
  const auto byteLength = static_cast<long>(bufferView.byteLength);

  if (byteOffset < 0 || byteLength < 0) {
    return Error::Unexpected("BufferView has negative byte offset or length.");
  }

  if (bufferView.bufferIndex < 0 ||
      static_cast<size_t>(bufferView.bufferIndex) >= asset.buffers.size()) {
    return Error::Unexpected("BufferView has invalid buffer index.");
  }

  if (bufferView.bufferIndex >= Buffers.size()) {
    return Error::Unexpected("BufferView buffer index out of bounds.");
  }

  if (static_cast<size_t>(byteOffset) + static_cast<size_t>(byteLength) >
      Buffers[bufferView.bufferIndex].size()) {
    return Error::Unexpected("BufferView byte range exceeds buffer size.");
  }

  auto &data = Buffers[bufferView.bufferIndex];

  return std::span<uint8_t>(data.data() + byteOffset, // NOLINT
                            byteLength);
}

inline auto LoadURI(const std::string_view &basePath,
                    const fastgltf::sources::URI &uriSource)
    -> Result<std::span<uint8_t>> {

  const auto &uri = uriSource.uri;
  const auto &path = Path::Join(basePath, uri.path());

  auto iter = URICache.find(path);
  if (iter != URICache.end()) {
    return iter->second;
  }
  PrintAlways("Loading URI: {}", path);

  {
    std::lock_guard<std::mutex> lock(URICacheMutex);
    auto data = CHECK_RES(Filesystem::ReadFile(path));

    URICache[path] = std::move(data);
    auto &cached = URICache[path];

    auto span = std::span<uint8_t>(cached.data(), cached.size()); // NOLINT
    return span;
  }
}

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const std::string_view &basePath,
                           const fastgltf::DataSource &dataSource)
    -> Result<std::span<uint8_t>> {
  ZoneScoped;

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
    return LoadURI(basePath, uriSource);
  }

  if (std::holds_alternative<fastgltf::sources::Array>(dataSource)) {
    const auto &arraySource = std::get<fastgltf::sources::Array>(dataSource);

    // NOLINTBEGIN

    auto dataSpan =
        std::span<uint8_t>(reinterpret_cast<uint8_t *>(const_cast<std::byte *>(
                               arraySource.bytes.data())),
                           arraySource.bytes.size());

    // NOLINTEND

    return dataSpan;
  }
  if (std::holds_alternative<fastgltf::sources::Vector>(dataSource)) {
    const auto &vectorSource = std::get<fastgltf::sources::Vector>(dataSource);

    // NOLINTBEGIN
    auto dataSpan =
        std::span<uint8_t>(reinterpret_cast<uint8_t *>(const_cast<std::byte *>(
                               vectorSource.bytes.data())),
                           vectorSource.bytes.size());
    // NOLINTEND

    return dataSpan;
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

    auto dataSpan =
        std::span<uint8_t>(reinterpret_cast<uint8_t *>(const_cast<std::byte *>(
                               byteViewSource.bytes.data())),
                           byteViewSource.bytes.size());
    // NOLINTEND

    return dataSpan;
  }

  return Error::Unexpected("Unsupported data source.");
}

inline auto LoadTexture(Graphics::GraphicsContext &context,
                        const fastgltf::Asset &asset,
                        const std::string_view &basePath,
                        const fastgltf::TextureInfo &gltfTexture)
    -> Result<Ref<Graphics::Texture>> {
  ZoneScoped;

  if (gltfTexture.textureIndex < TextureCache.size() &&
      TextureCache[gltfTexture.textureIndex].isValid()) {
    return TextureCache[gltfTexture.textureIndex];
  }

  const auto &texture = asset.textures[gltfTexture.textureIndex];
  const auto &sampler = texture.samplerIndex.has_value()
                            ? asset.samplers[texture.samplerIndex.value()]
                            : fastgltf::Sampler{};

  if (!texture.imageIndex.has_value()) {
    return Error::Unexpected("Texture has no image index.");
  }

  const auto &gltfimage = asset.images[texture.imageIndex.value()];

  auto span = CHECK_RES(LoadDataSource(asset, basePath, gltfimage.data));
  Ref<Graphics::Texture> textureRef;

  const auto &image = ImageCache[texture.imageIndex.value()];

  if (std::holds_alternative<Ref<Image::ImageData>>(image)) {
    textureRef = CHECK_RES(Graphics::Texture::FromMemory(
        context, *std::get<Ref<Image::ImageData>>(image).get(),
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        Graphics::TextureMipmapOption::Init));
  } else if (std::holds_alternative<Ref<Image::CompressedImageData>>(image)) {
    textureRef = CHECK_RES(Graphics::Texture::FromMemory(
        context, *std::get<Ref<Image::CompressedImageData>>(image).get(),
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        Graphics::TextureMipmapOption::Manual));
  } else {
    return Error::Unexpected("Invalid image type in cache.");
  }

  textureRef->SetLodRange(0.0F, (float)textureRef->GetMipmapCount() - 1);
  textureRef->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                        VK_SAMPLER_MIPMAP_MODE_LINEAR);

  if (TextureCache.size() <= gltfTexture.textureIndex) {
    TextureCache.resize(gltfTexture.textureIndex + 100UL); // NOLINT
  }

  TextureCache[gltfTexture.textureIndex] = textureRef;

  return textureRef;
}

inline auto LoadMaterial(Graphics::GraphicsContext &context,
                         const fastgltf::Asset &asset,
                         const std::string_view &basePath,
                         const fastgltf::Material &gltfMaterial,
                         Ref<Engine::Renderer::LuaMaterial> &luaMaterial)
    -> Error {
  ZoneScoped;

  // auto &material = luaMaterial->material;
  auto *material =
      luaMaterial->entity.try_get_mut<Engine::Renderer::Material>();

  if (material == nullptr) {
    return Error::Createf(
        "Failed to get mutable reference to Material component.");
  }

  material->name = gltfMaterial.name;

  material->cullMode =
      gltfMaterial.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

  material->alphaCutoff = gltfMaterial.alphaCutoff;
  material->roughnessFactor = gltfMaterial.pbrData.roughnessFactor;
  material->metallicFactor = gltfMaterial.pbrData.metallicFactor;
  material->albedoFactor = Math::Vec4(gltfMaterial.pbrData.baseColorFactor[0],
                                      gltfMaterial.pbrData.baseColorFactor[1],
                                      gltfMaterial.pbrData.baseColorFactor[2],
                                      gltfMaterial.pbrData.baseColorFactor[3]);

  // The alpha mode is exactly the same enum as our AlphaMode.
  // But for type safety, we do a manual mapping.

  switch (gltfMaterial.alphaMode) {
  case fastgltf::AlphaMode::Opaque:
    material->alphaMode = Engine::Renderer::AlphaMode::Opaque;
    break;
  case fastgltf::AlphaMode::Mask:
    material->alphaMode = Engine::Renderer::AlphaMode::Mask;
    break;
  case fastgltf::AlphaMode::Blend:
    material->alphaMode = Engine::Renderer::AlphaMode::Blend;
    break;
  }

  material->emissiveFactor =
      Math::Vec3(gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1],
                 gltfMaterial.emissiveFactor[2]);

  if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
    material->albedoTexture =
        CHECK_RES(LoadTexture(context, asset, basePath,
                              gltfMaterial.pbrData.baseColorTexture.value()));
  }

  if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
    material->metallicRoughnessTexture = CHECK_RES(
        LoadTexture(context, asset, basePath,
                    gltfMaterial.pbrData.metallicRoughnessTexture.value()));
  }

  if (gltfMaterial.occlusionTexture.has_value()) {
    material->ambientOcclusionTexture = CHECK_RES(LoadTexture(
        context, asset, basePath, gltfMaterial.occlusionTexture.value()));
  }

  if (gltfMaterial.normalTexture.has_value()) {
    material->normalTexture = CHECK_RES(LoadTexture(
        context, asset, basePath, gltfMaterial.normalTexture.value()));
  }

  if (gltfMaterial.emissiveTexture.has_value()) {
    material->emissiveTexture = CHECK_RES(LoadTexture(
        context, asset, basePath, gltfMaterial.emissiveTexture.value()));
  }

  return Error::Success();
}

auto ComponentTypeToString(fastgltf::ComponentType type) -> std::string_view {
  switch (type) {
  case fastgltf::ComponentType::Byte:
    return "Byte";
  case fastgltf::ComponentType::UnsignedByte:
    return "UnsignedByte";
  case fastgltf::ComponentType::Short:
    return "Short";
  case fastgltf::ComponentType::UnsignedShort:
    return "UnsignedShort";
  case fastgltf::ComponentType::Int:
    return "Int";
  case fastgltf::ComponentType::UnsignedInt:
    return "UnsignedInt";
  case fastgltf::ComponentType::Float:
    return "Float";
  case fastgltf::ComponentType::Double:
    return "Double";
  default:
    return "Invalid";
  }
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
    auto count = static_cast<int>(fastgltf::getNumComponents(accessor.type));

    auto formatResult = GetVkFormat(accessor.componentType, count);

    if (Error::IsError(formatResult)) {
      return formatResult.error();
    }

    Graphics::VertexComponent attribute{};
    attribute.format = formatResult.value();
    attribute.name = semantic;
    attribute.binding = 0;
    attributes.emplace_back(attribute);
  }

  return Graphics::VertexFormat(attributes);
}

/// Normalize an integer attribute value to float [0,1] or [-1,1].
/// Writes `compCount` floats (each 4 bytes) into `dst`.
inline void NormalizeAttribute(const uint8_t *src, uint8_t *dst,
                               fastgltf::ComponentType compType,
                               size_t compCount) {

  const float IntToFloat = 1.0F / 127.0F;
  const float UIntToFloat = 1.0F / 255.0F;
  const float ShortToFloat = 1.0F / 32767.0F;
  const float UShortToFloat = 1.0F / 65535.0F;

  for (size_t i = 0; i < compCount; ++i) {
    float value = 0.0F;
    switch (compType) {
    case fastgltf::ComponentType::Byte: {
      auto cvalue = static_cast<int8_t>(src[i]); // NOLINT
      value = std::max(static_cast<float>(cvalue) * IntToFloat, -1.0F);
      break;
    }
    case fastgltf::ComponentType::UnsignedByte: {
      value = static_cast<float>(src[i]) * UIntToFloat; // NOLINT
      break;
    }
    case fastgltf::ComponentType::Short: {
      int16_t cvalue{};
      memcpy(&cvalue, src + (i * 2), 2); // NOLINT
      value = std::max(static_cast<float>(cvalue) * ShortToFloat, -1.0F);
      break;
    }
    case fastgltf::ComponentType::UnsignedShort: {
      uint16_t cvalue{};
      memcpy(&cvalue, src + (i * 2), 2); // NOLINT
      value = static_cast<float>(cvalue) * UShortToFloat;
      break;
    }
    default:
      break;
    }
    memcpy(dst + (i * sizeof(float)), &value, sizeof(float)); // NOLINT
  }
}

static inline auto PackToSigned10Bit(float value) -> uint32_t {
  value = std::max(std::min(value, 1.0F), -1.0F);
  value = (value * 0.5F) + 0.5F; // Map from [-1,1] to [0,1] NOLINT
  auto intValue = static_cast<int32_t>(std::round(value * 1023.0F)); // NOLINT
  return static_cast<uint32_t>(intValue) & 0x3FF;                    // NOLINT
}

static void ConvertNormalToPacked10Bit(const uint8_t *src, uint8_t *dst) {
  float normalX = 0.0F;
  float normalY = 0.0F;
  float normalZ = 0.0F;
  memcpy(&normalX, src, sizeof(float));                       // NOLINT
  memcpy(&normalY, src + sizeof(float), sizeof(float));       // NOLINT
  memcpy(&normalZ, src + (2 * sizeof(float)), sizeof(float)); // NOLINT

  uint32_t packedX = PackToSigned10Bit(normalX);
  uint32_t packedY = PackToSigned10Bit(normalY);
  uint32_t packedZ = PackToSigned10Bit(normalZ);

  uint32_t packedNormal =
      (packedX) | (packedY << 10) | (packedZ << 20); // NOLINT

  memcpy(dst, &packedNormal, sizeof(uint32_t)); // NOLINT
}

static void ConvertTangentToPacked10Bit(const uint8_t *src, uint8_t *dst) {
  float tangentX = 0.0F;
  float tangentY = 0.0F;
  float tangentZ = 0.0F;
  float tangentW = 0.0F;
  memcpy(&tangentX, src, sizeof(float));                       // NOLINT
  memcpy(&tangentY, src + sizeof(float), sizeof(float));       // NOLINT
  memcpy(&tangentZ, src + (2 * sizeof(float)), sizeof(float)); // NOLINT
  memcpy(&tangentW, src + (3 * sizeof(float)), sizeof(float)); // NOLINT

  uint32_t packedX = PackToSigned10Bit(tangentX);
  uint32_t packedY = PackToSigned10Bit(tangentY);
  uint32_t packedZ = PackToSigned10Bit(tangentZ);
  uint32_t packedW = tangentW > 0.0F ? 1 : 0; // Store sign in W

  uint32_t packedTangent =
      (packedX) | (packedY << 10) | (packedZ << 20) | (packedW << 30); // NOLINT

  memcpy(dst, &packedTangent, sizeof(uint32_t)); // NOLINT
}

static auto PackToUnsigned8Bit(float value) -> uint {
  value = std::max(std::min(value, 1.0F), 0.0F);
  return static_cast<uint32_t>(std::round(value * 255.0F)); // NOLINT
}

static auto ConvertColorToPacked8Bit(const uint8_t *src, uint8_t *dst) -> void {
  float colorR = 0.0F;
  float colorG = 0.0F;
  float colorB = 0.0F;
  float colorA = 1.0F;
  memcpy(&colorR, src, sizeof(float));                       // NOLINT
  memcpy(&colorG, src + sizeof(float), sizeof(float));       // NOLINT
  memcpy(&colorB, src + (2 * sizeof(float)), sizeof(float)); // NOLINT
  memcpy(&colorA, src + (3 * sizeof(float)), sizeof(float)); // NOLINT

  uint32_t packedR = PackToUnsigned8Bit(colorR);
  uint32_t packedG = PackToUnsigned8Bit(colorG);
  uint32_t packedB = PackToUnsigned8Bit(colorB);
  uint32_t packedA = PackToUnsigned8Bit(colorA);

  uint32_t packedColor =
      (packedR) | (packedG << 8) | (packedB << 16) | (packedA << 24); // NOLINT

  memcpy(dst, &packedColor, sizeof(uint32_t));
}

// Converter functions; for example, float32vec3 to packed uint 10-bit per component.
// The key is the semantic, and the value is a function that takes the source data and writes the converted data to the destination buffer.
const std::unordered_map<std::string,
                         std::function<void(const uint8_t *src, uint8_t *dst)>>
    Converters = {
        {"NORMAL", ConvertNormalToPacked10Bit},
        {"TANGENT", ConvertTangentToPacked10Bit},
        {"POSITION",
         nullptr}, // No conversion for POSITION, but could add one if desired.
        {"COLOR_0", ConvertColorToPacked8Bit},
        {"TEXCOORD_0", nullptr}

};

inline auto TriangleTangent(Math::Vec3 vert0, Math::Vec3 vert1,
                            Math::Vec3 vert2, Math::Vec2 uv0, Math::Vec2 uv1,
                            Math::Vec2 uv2) -> Math::Vec4 {
  Math::Vec3 edge1 = vert1 - vert0;
  Math::Vec3 edge2 = vert2 - vert0;
  Math::Vec2 deltaUV1 = uv1 - uv0;
  Math::Vec2 deltaUV2 = uv2 - uv0;

  float factor = 1.0F / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

  Math::Vec4 tangent{};
  tangent.x = factor * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x); // NOLINT
  tangent.y = factor * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y); // NOLINT
  tangent.z = factor * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z); // NOLINT
  tangent.w = 1.0F;

  return tangent.Normalize();
}

// NOLINTNEXTLINE
inline auto FillVertexDataDefaults(Graphics::VertexFormat &format,
                                   const fastgltf::Asset &asset,
                                   const fastgltf::Primitive &primitive,
                                   std::vector<uint8_t> &existingData,
                                   const std::vector<uint8_t> &indices,
                                   VkIndexType indexType) -> void {

  auto stride = format.GetStride(0);

  // Check for missing attributes and fill with defaults if needed.
  for (const auto &component : format.GetAttributes()) {
    bool hasAttribute = false;
    for (const auto &[semantic, accessorIndex] : primitive.attributes) {
      auto semanticView = std::string_view(semantic);
      if (semanticView == component.name) {
        hasAttribute = true;
        break;
      }
    }

    if (hasAttribute) {
      continue;
    }

    if (component.name == "TANGENT") {
      continue;

      auto writeOffset = component.offset;
      uint32_t positionOffset{};
      uint32_t uvOffset{};

      // Find POSITION and TEXCOORD_0 offsets.
      for (const auto &comp : format.GetAttributes()) {
        if (comp.name == "POSITION") {
          positionOffset = comp.offset;
        } else if (comp.name == "TEXCOORD_0") {
          uvOffset = comp.offset;
        }
      }

      if (indexType == VK_INDEX_TYPE_MAX_ENUM) {
#pragma unroll 2
        for (size_t vertex = 0; vertex < existingData.size() / stride;
             vertex += 3) {
          auto vert0 = existingData.data() + vertex * stride;       // NOLINT
          auto vert1 = existingData.data() + (vertex + 1) * stride; // NOLINT
          auto vert2 = existingData.data() + (vertex + 2) * stride; // NOLINT

          Math::Vec3 pos0{};
          Math::Vec3 pos1{};
          Math::Vec3 pos2{};
          Math::Vec2 uv0{};
          Math::Vec2 uv1{};
          Math::Vec2 uv2{};

          memcpy(&pos0, vert0 + positionOffset, sizeof(Math::Vec3)); // NOLINT
          memcpy(&uv0, vert0 + uvOffset, sizeof(Math::Vec2));        // NOLINT
          memcpy(&pos1, vert1 + positionOffset, sizeof(Math::Vec3)); // NOLINT
          memcpy(&uv1, vert1 + uvOffset, sizeof(Math::Vec2));        // NOLINT
          memcpy(&pos2, vert2 + positionOffset, sizeof(Math::Vec3)); // NOLINT
          memcpy(&uv2, vert2 + uvOffset, sizeof(Math::Vec2));        // NOLINT

          Math::Vec4 tangent = TriangleTangent(pos0, pos1, pos2, uv0, uv1, uv2);

          ConvertTangentToPacked10Bit(
              reinterpret_cast<const uint8_t *>(&tangent), // NOLINT
              vert0 + writeOffset);                        // NOLINT
        }
      } else {
        size_t indexSize = indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
        size_t triangleCount = indices.size() / (3 * indexSize);
#pragma unroll 2
        for (size_t triangle = 0; triangle < triangleCount; ++triangle) {
          uint32_t index0{};
          uint32_t index1{};
          uint32_t index2{};

          if (indexType == VK_INDEX_TYPE_UINT16) {
            index0 = *reinterpret_cast<const uint16_t *>(         // NOLINT
                indices.data() + triangle * 3 * indexSize);       // NOLINT
            index1 = *reinterpret_cast<const uint16_t *>(         // NOLINT
                indices.data() + (triangle * 3 + 1) * indexSize); // NOLINT
            index2 = *reinterpret_cast<const uint16_t *>(         // NOLINT
                indices.data() + (triangle * 3 + 2) * indexSize); // NOLINT
          } else {
            index0 = *reinterpret_cast<const uint32_t *>(         // NOLINT
                indices.data() + triangle * 3 * indexSize);       // NOLINT
            index1 = *reinterpret_cast<const uint32_t *>(         // NOLINT
                indices.data() + (triangle * 3 + 1) * indexSize); // NOLINT
            index2 = *reinterpret_cast<const uint32_t *>(         // NOLINT
                indices.data() + (triangle * 3 + 2) * indexSize); // NOLINT
          }

          auto vert0 = existingData.data() + index0 * stride; // NOLINT
          auto vert1 = existingData.data() + index1 * stride; // NOLINT
          auto vert2 = existingData.data() + index2 * stride; // NOLINT

          Math::Vec3 pos0{};
          Math::Vec3 pos1{};
          Math::Vec3 pos2{};
          Math::Vec2 uv0{};
          Math::Vec2 uv1{};
          Math::Vec2 uv2{};

          memcpy(&pos0, vert0 + positionOffset, sizeof(Math::Vec3)); // NOLINT
          memcpy(&uv0, vert0 + uvOffset, sizeof(Math::Vec2));        // NOLINT
          memcpy(&pos1, vert1 + positionOffset, sizeof(Math::Vec3)); // NOLINT
          memcpy(&uv1, vert1 + uvOffset, sizeof(Math::Vec2));        // NOLINT
          memcpy(&pos2, vert2 + positionOffset, sizeof(Math::Vec3)); // NOLINT
          memcpy(&uv2, vert2 + uvOffset, sizeof(Math::Vec2));        // NOLINT

          Math::Vec4 tangent = TriangleTangent(pos0, pos1, pos2, uv0, uv1, uv2);

          memcpy(vert0 + writeOffset, &tangent, sizeof(Math::Vec4)); // NOLINT
          memcpy(vert1 + writeOffset, &tangent, sizeof(Math::Vec4)); // NOLINT
          memcpy(vert2 + writeOffset, &tangent, sizeof(Math::Vec4)); // NOLINT
        }
      }
    }
    if (component.name == "COLOR_0") {
      // Fill missing vertex colors with white (1,1,1,1).
      uint32_t whiteColor = ~0U; // RGBA packed white
      for (size_t vertex = 0; vertex < existingData.size() / stride; ++vertex) {
        auto vert = existingData.data() + vertex * stride; // NOLINT
        memcpy(vert + component.offset, &whiteColor,       // NOLINT
               sizeof(uint32_t));
      }
    } else {
      // For other attributes, NOOP.
    }
  }
}

inline auto // NOLINTNEXTLINE
LoadVertexData(Graphics::VertexFormat &format, const fastgltf::Asset &asset,
               const fastgltf::Primitive &primitive,
               const std::vector<uint8_t> &indices, VkIndexType indexType)
    -> Result<std::vector<uint8_t>> {
  if (primitive.attributes.empty()) {
    return Error::Unexpected("Primitive has no attributes.");
  }

  // Determine vertex count from first accessor.
  const size_t vertexCount =
      asset.accessors.at(primitive.attributes.begin()->accessorIndex).count;

  // Compute output stride from the vertex format.
  const size_t outputStride = format.GetStride(0);

  std::vector<uint8_t> result(vertexCount * outputStride, 0);

  for (const auto &[semantic, accessorIndex] : primitive.attributes) {
    const fastgltf::Accessor &accessor = asset.accessors.at(accessorIndex);
    std::string_view semanticView(semantic);

    // Find matching vertex component in format.
    const Graphics::VertexComponent *attribute = nullptr;
    for (const auto &component : format.GetAttributes()) {
      if (component.name == semanticView) {
        attribute = &component;
        break;
      }
    }

    if (attribute == nullptr || attribute->format == VK_FORMAT_UNDEFINED) {
      // return Error::Unexpectedf("Couldn't match Format for: {}", semantic);
      continue; // Skip attributes that aren't in the vertex format.
    }

    if (accessor.count != vertexCount) {
      return Error::Unexpectedf(
          "Accessor count mismatch for {}: expected {}, got {}", semantic,
          vertexCount, accessor.count);
    }

    if (!accessor.bufferViewIndex.has_value()) {
      return Error::Unexpectedf("Accessor for {} has no bufferView.", semantic);
    }

    if (accessor.sparse.has_value()) {
      return Error::Unexpectedf("Sparse accessors not supported for {}.",
                                semantic);
    }

    const auto &bufferView =
        asset.bufferViews.at(accessor.bufferViewIndex.value());

    auto span = CHECK_RES(LoadBufferView(asset, bufferView));

    const auto srcComponentSize =
        fastgltf::getComponentByteSize(accessor.componentType);
    const auto componentCount =
        static_cast<size_t>(fastgltf::getNumComponents(accessor.type));
    const size_t srcElementSize = srcComponentSize * componentCount;
    auto vkFormatSize = Graphics::Format::GetSize(attribute->format);

    // byteStride of 0 means tightly packed.
    const size_t srcStride = bufferView.byteStride.value_or(srcElementSize);

    if (srcStride < srcElementSize) {
      return Error::Unexpectedf(
          "Buffer view stride too small for {}: need at least {}, got {}",
          semanticView, srcElementSize, srcStride);
    }

    // Bounds check: ensure all vertex data fits within the buffer.
    const size_t requiredSize =
        (vertexCount > 0 ? ((vertexCount - 1) * srcStride) + srcElementSize +
                               accessor.byteOffset
                         : 0);

    auto spansize = span.size();

    // Check if the accessor's data fits within the buffer view span.
    if (requiredSize > spansize) {
      return Error::Unexpectedf(
          "Buffer too small for attribute {}: need {}, have {}", semanticView,
          requiredSize, spansize);
    }

    const size_t dstElementSize = Graphics::Format::GetSize(attribute->format);
    const size_t dstOffset = attribute->offset;

    // Check if we need normalization (accessor.normalized + integer type).
    const bool needsNormalize =
        accessor.normalized &&
        (accessor.componentType == fastgltf::ComponentType::Byte ||
         accessor.componentType == fastgltf::ComponentType::UnsignedByte ||
         accessor.componentType == fastgltf::ComponentType::Short ||
         accessor.componentType == fastgltf::ComponentType::UnsignedShort);

    if (needsNormalize && srcElementSize != dstElementSize) {
      return Error::Unexpectedf(
          "Normalization requires matching src and dst element sizes for {}: "
          "got src {}, dst {}",
          semanticView, srcElementSize, dstElementSize);
    }

    auto resultSize = result.size();
    auto finalWriteSize =
        ((vertexCount - 1) * outputStride) + dstOffset + dstElementSize;
    if (finalWriteSize > resultSize) {
      return Error::Unexpectedf(
          "Output buffer too small for attribute {}: need {}, have {}",
          semanticView, finalWriteSize, resultSize);
    }

    auto converterIter = Converters.find(std::string(semantic));
    std::function<void(const uint8_t *src, uint8_t *dst)> converter = nullptr;
    if (converterIter != Converters.end()) {
      converter = converterIter->second;
    }

    for (size_t value = 0; value < vertexCount; ++value) {
      const uint8_t *srcPtr =
          span.data() + value * srcStride + accessor.byteOffset; // NOLINT
      uint8_t *dstPtr =
          result.data() + value * outputStride + dstOffset; // NOLINT

      if (needsNormalize) {
        NormalizeAttribute(srcPtr, dstPtr, accessor.componentType,
                           componentCount);
      } else {
        if (converter != nullptr) {
          converter(srcPtr, dstPtr);
        } else {
          memcpy(dstPtr, srcPtr, srcElementSize);
        }
      }
    }
  }

  FillVertexDataDefaults(format, asset, primitive, result, indices, indexType);

  return result;
}

inline auto LoadIndexData(const fastgltf::Asset &asset,
                          const fastgltf::Primitive &primitive)
    -> Result<std::vector<uint8_t>> {
  if (!primitive.indicesAccessor.has_value()) {
    return std::vector<uint8_t>{};
  }

  const auto &accessor = asset.accessors.at(primitive.indicesAccessor.value());

  assert(accessor.bufferViewIndex.has_value());
  const auto &bufferView =
      asset.bufferViews.at(accessor.bufferViewIndex.value());

  auto span = CHECK_RES(LoadBufferView(asset, bufferView));

  auto componentSize = fastgltf::getComponentByteSize(accessor.componentType);
  auto finalOffset = accessor.byteOffset;
  auto finalLength = accessor.count * componentSize;

  // NOLINTNEXTLINE
  return std::vector<uint8_t>(span.data() + finalOffset,
                              span.data() + finalOffset + // NOLINT
                                  finalLength);
}

template <typename IndexT>
void OptimizeMesh(std::span<IndexT> indices, std::span<uint8_t> vertices,
                  size_t vertexCount, size_t vertexStride) {

  size_t indexCount = indices.size();

  std::vector<unsigned int> remap(vertexCount);

  size_t newVertexCount =
      meshopt_generateVertexRemap(remap.data(), indices.data(), indexCount,
                                  vertices.data(), vertexCount, vertexStride);

  meshopt_remapIndexBuffer(indices.data(), indices.data(), indexCount,
                           remap.data());

  meshopt_remapVertexBuffer(vertices.data(), vertices.data(), vertexCount,
                            vertexStride, remap.data());

  meshopt_optimizeVertexCache(indices.data(), indices.data(), indexCount,
                              vertexCount);

  meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indexCount,
                              vertices.data(), vertexCount, vertexStride);
}

auto GetInterleavedStride(const Graphics::VertexFormat &format) -> size_t {
  size_t stride = 0;
  for (int i = 0; i < format.GetBindingCount(); i++) {
    stride += format.GetStride(i);
  }
  return stride;
}

auto DeinterleaveVertexData(const std::vector<uint8_t> &interleavedData)
    -> std::vector<std::vector<uint8_t>> {

  struct VertexData {
    float position[3]; // NOLINT
    float texcoord[2]; // NOLINT
    uint32_t normal;
    uint32_t tangent;
    uint32_t color;
  };

  size_t vertexCount = interleavedData.size() / sizeof(VertexData);

  const auto *vertexArray = // NOLINTNEXTLINE
      reinterpret_cast<const VertexData *>(interleavedData.data());

  auto positionStride = sizeof(float) * 3;
  auto texcoordStride = sizeof(float) * 2;
  auto normalTangentStride = sizeof(uint32_t) * 2;
  auto colorStride = sizeof(uint32_t);

  auto positionData = std::vector<uint8_t>(vertexCount * positionStride);
  auto texcoordData = std::vector<uint8_t>(vertexCount * texcoordStride);
  auto normalTangentData =
      std::vector<uint8_t>(vertexCount * normalTangentStride);
  auto colorData = std::vector<uint8_t>(vertexCount * colorStride);

  for (int vertex = 0; vertex < vertexCount; ++vertex) {
    const auto &srcVertex = vertexArray[vertex]; // NOLINT

    // NOLINTNEXTLINE
    memcpy(positionData.data() + (vertex * positionStride), &srcVertex.position,
           positionStride);
    // NOLINTNEXTLINE
    memcpy(texcoordData.data() + (vertex * texcoordStride), &srcVertex.texcoord,
           texcoordStride);
    // NOLINTNEXTLINE
    memcpy(normalTangentData.data() + (vertex * normalTangentStride),
           &srcVertex.normal, normalTangentStride);
    // NOLINTNEXTLINE
    memcpy(colorData.data() + (vertex * colorStride), &srcVertex.color,
           colorStride);
  }

  return {positionData, texcoordData, normalTangentData, colorData};
}

// NOLINTNEXTLINE
inline auto LoadNode(flecs::world *world, Graphics::GraphicsContext &context,
                     const fastgltf::Asset &asset,
                     const std::string_view &basePath,
                     const fastgltf::Node &gltfNode)
    -> Result<std::vector<flecs::entity>> {
  bool isMesh = gltfNode.meshIndex.has_value();
  bool isSkin = gltfNode.skinIndex.has_value();
  bool isCamera = gltfNode.cameraIndex.has_value();
  bool isLight = gltfNode.lightIndex.has_value();

  ZoneScoped;

  // Note: checking if children aren't empty might cull some nodes, should check if this is an issue.
  bool isNode = !isMesh && !isSkin && !isCamera && !isLight;

  Engine::Transform transform{};

  if (std::holds_alternative<fastgltf::TRS>(gltfNode.transform)) {
    const auto &trs = std::get<fastgltf::TRS>(gltfNode.transform);

    transform.SetPosition(trs.translation[0], trs.translation[1],
                          trs.translation[2]);
    transform.SetRotation(trs.rotation[0], trs.rotation[1], trs.rotation[2],
                          trs.rotation[3]);
    transform.SetScale(trs.scale[0], trs.scale[1], trs.scale[2]);
  } else {
    // Shouldn't be hit. Since we specified DecomposeNodeMatrices, all matrices should
    // have been decomposed.
    return Error::Unexpected("Attempted to load a node with matrix transform.");
  }

  if (isNode) {
    auto node = world->entity(gltfNode.name.c_str());
    node.add<Engine::Node>();
    node.add<Engine::Transform>();
    node.add<Engine::Userdata>();
    node.set<Engine::Transform>(transform);
    node.add<Engine::WorldBounds>();

    for (const auto &childIndex : gltfNode.children) {
      const auto &childGltfNode = asset.nodes[childIndex];
      auto childNode =
          CHECK_RES(LoadNode(world, context, asset, basePath, childGltfNode));

      for (auto &childObject : childNode) {
        childObject.child_of(node);
      }
    }

    return std::vector<flecs::entity>{node};
  }

  if (isMesh) {
    auto meshIndex = *gltfNode.meshIndex;
    const auto &gltfMesh = asset.meshes[meshIndex];

    std::vector<flecs::entity> shapes;

    for (const auto &primitive : gltfMesh.primitives) {
      // Each primitive corresponds to a separate mesh in our engine.
      auto shapeEntity =
          world->entity(GetUniqueName(gltfMesh.name.c_str()).c_str());
      shapeEntity.add<Engine::Shape>();
      shapeEntity.add<Engine::WorldBounds>();
      // shapeEntity.add<Engine::LocalBounds>(); No local bounds, only based on child geometry bounds.
      shapeEntity.set<Engine::Transform>(transform);

      shapes.emplace_back(shapeEntity);

      auto indexData = CHECK_RES(LoadIndexData(asset, primitive));
      VkIndexType indexType = VK_INDEX_TYPE_MAX_ENUM;
      if (!indexData.empty()) {
        const auto &accessor =
            asset.accessors.at(primitive.indicesAccessor.value());
        switch (accessor.componentType) {
        case fastgltf::ComponentType::UnsignedByte:
          indexType = VK_INDEX_TYPE_UINT8_EXT;
          break;
        case fastgltf::ComponentType::UnsignedShort:
          indexType = VK_INDEX_TYPE_UINT16;
          break;
        case fastgltf::ComponentType::UnsignedInt:
          indexType = VK_INDEX_TYPE_UINT32;
          break;
        default:
          return Error::Unexpected("Unsupported index component type.");
        }
      }

      using Comp = Graphics::VertexComponent;

      static std::vector<Comp> InitialVertexComponents = {
          Comp{.name = "POSITION",
               .location = 0,
               .binding = 0,
               .format = VK_FORMAT_R32G32B32_SFLOAT},
          Comp{.name = "TEXCOORD_0",
               .location = 1,
               .binding = 0,
               .format = VK_FORMAT_R32G32_SFLOAT},
          Comp{.name = "NORMAL",
               .location = 2,
               .binding = 0,
               .format = VK_FORMAT_R32_UINT},
          Comp{.name = "TANGENT",
               .location = 3,
               .binding = 0,
               .format = VK_FORMAT_R32_UINT},
          Comp{.name = "COLOR_0",
               .location = 4,
               .binding = 0,
               .format = VK_FORMAT_R8G8B8A8_UNORM}};

      static std::vector<Comp> SeparateVertexComponents =
          InitialVertexComponents;

      SeparateVertexComponents.at(0).binding = 0; // POSITION
      SeparateVertexComponents.at(1).binding = 1; // TEXCOORD_0
      SeparateVertexComponents.at(2).binding = 2; // NORMAL + TANGENT
      SeparateVertexComponents.at(3).binding = 2; // NORMAL + TANGENT
      SeparateVertexComponents.at(4).binding = 3; // COLOR_0

      struct VertexData {
        float position[3]; // NOLINT
        float texcoord[2]; // NOLINT
        uint32_t normal;
        uint32_t tangent;
        uint32_t color;
      };

      Graphics::VertexFormat InitialVertexFormat(InitialVertexComponents);

      auto &vertexFormat = InitialVertexFormat;
      auto vertexData = CHECK_RES(
          LoadVertexData(vertexFormat, asset, primitive, indexData, indexType));

      auto stride = vertexFormat.GetStride(0);
      auto vertexCount = vertexData.size() / stride;

      switch (indexType) {
      case VK_INDEX_TYPE_UINT8:
        OptimizeMesh( // NOLINTNEXTLINE
            std::span(reinterpret_cast<uint8_t *>(indexData.data()),
                      indexData.size() / sizeof(uint8_t)),
            vertexData, vertexCount, stride);
        break;
      case VK_INDEX_TYPE_UINT16:
        OptimizeMesh( // NOLINTNEXTLINE
            std::span(reinterpret_cast<uint16_t *>(indexData.data()),
                      indexData.size() / sizeof(uint16_t)),
            vertexData, vertexCount, stride);
        break;
      case VK_INDEX_TYPE_UINT32:
        OptimizeMesh( // NOLINTNEXTLINE
            std::span(reinterpret_cast<uint32_t *>(indexData.data()),
                      indexData.size() / sizeof(uint32_t)),
            vertexData, vertexCount, stride);
        break;
      case VK_INDEX_TYPE_NONE_KHR:
      case VK_INDEX_TYPE_MAX_ENUM:
        break;
      }

      static Graphics::VertexFormat SeparateVertexFormat(
          SeparateVertexComponents);

      auto mesh = CHECK_RES(
          Graphics::Mesh::Create(context, SeparateVertexFormat, vertexCount,
                                 std::string(gltfMesh.name)));

      if (!indexData.empty()) {
        CHECK_ERR(mesh->SetIndices(context, indexData, indexType));
      }

      auto deinterleavedData = DeinterleaveVertexData(vertexData);

      // clang-format off
      CHECK_ERR(mesh->SetVertices(context, 0, deinterleavedData[0])); // POSITION
      CHECK_ERR(mesh->SetVertices(context, 1, deinterleavedData[1])); // TEXCOORD_0
      CHECK_ERR(mesh->SetVertices(context, 2, deinterleavedData[2])); // NORMAL + TANGENT
      CHECK_ERR(mesh->SetVertices(context, 3, deinterleavedData[3])); // COLOR
      // clang-format on

      auto geometry = world->entity(
          GetUniqueName(std::string(gltfMesh.name) + " Geometry").c_str());
      geometry.set<Engine::Geometry>(Engine::Geometry{mesh});
      geometry.add<Engine::Transform>();

      auto lod = world->entity(
          GetUniqueName(std::string(gltfMesh.name) + " LOD").c_str());

      Engine::LocalBounds localBounds{};

      std::span<const VertexData> vertexDataSpan(
          reinterpret_cast<const VertexData *>(vertexData.data()), // NOLINT
          vertexData.size() / sizeof(VertexData));

      for (const auto &vertex : vertexDataSpan) {
        Math::Vec3 pos(vertex.position[0], vertex.position[1],
                       vertex.position[2]);
        localBounds.Bounds.Grow(pos);
      }

      lod.add<Engine::LevelOfDetail>();
      lod.add<Engine::WorldBounds>();
      lod.add<Engine::Transform>();

      geometry.set<Engine::LocalBounds>(localBounds);
      geometry.add<Engine::WorldBounds>();

      geometry.child_of(lod);
      lod.child_of(shapeEntity);

      // Load material if present.
      if (primitive.materialIndex.has_value()) {
        const auto &material = asset.materials[primitive.materialIndex.value()];
        geometry.add<Engine::Renderer::Material>();
        auto rendererMaterial =
            Ref<Engine::Renderer::LuaMaterial>::Make(geometry);

        CHECK_ERR(
            LoadMaterial(context, asset, basePath, material, rendererMaterial));
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path,
                   flecs::world *world) -> Error {
  /// Load the file into a data buffer.

  auto bytes = CHECK_RES(Filesystem::ReadFile(path));
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
    return Error::Create(fastgltf::getErrorName(error));
  }

  /// Validate the loaded asset.
  auto validationError = fastgltf::validate(asset.get());

  if (validationError != fastgltf::Error::None) {
    return Error::Create("glTF asset validation failed: " +
                         std::string(fastgltf::getErrorName(validationError)));
  }

  Buffers.reserve(asset->buffers.size());
  const auto &basePath = Path::Directory(path);
  const auto view = std::string_view(basePath);

  for (size_t i = 0; i < asset->buffers.size(); ++i) {
    const auto &buffer = asset->buffers[i];
    auto bufferData = CHECK_RES(LoadDataSource(asset.get(), view, buffer.data));
    Buffers.emplace_back(bufferData.begin(), bufferData.end());
  }

  std::vector<size_t> indices(asset->images.size());
  std::ranges::iota(indices, 0);
  ImageCache.resize(asset->images.size());
  Error imageLoadError = Error::Success();

  std::for_each(std::execution::par, indices.begin(), indices.end(),
                [&](const auto &index) -> auto {
                  const fastgltf::Image &image = asset->images[index];
                  auto loadResult =
                      LoadDataSource(asset.get(), basePath, image.data);
                  if (Error::IsError(loadResult)) {
                    imageLoadError = loadResult.error();
                    return;
                  }

                  auto span = loadResult.value();

                  if (Image::IsDDS(span)) {
                    auto imageDataResult =
                        Image::CompressedImageData::Create(span);
                    if (Error::IsError(imageDataResult)) {
                      imageLoadError = imageDataResult.error();
                      return;
                    }
                    ImageCache.at(index) = imageDataResult.value();
                  } else {
                    auto imageDataResult = Image::ImageData::Create(span);
                    if (Error::IsError(imageDataResult)) {
                      imageLoadError = imageDataResult.error();
                      return;
                    }

                    ImageCache.at(index) = imageDataResult.value();
                  }
                });

  if (Error::IsError(imageLoadError)) {
    return imageLoadError;
  }

  PrintWarning("Loaded {} images for glTF asset.", ImageCache.size());

  // loop through scenes
  for (const auto &glTFScene : asset->scenes) {
    for (const auto &nodeIndex : glTFScene.nodeIndices) {
      const auto &gltfNode = asset->nodes[nodeIndex];
      CHECK_RES(LoadNode(world, context, asset.get(), view, gltfNode));
    }
  }

  {
    std::lock_guard<std::mutex> lock(URICacheMutex);

    Buffers.clear();
    URICache.clear();
    ImageCache.clear();
    TextureCache.clear();
  }

  return Error::Success();
}

} // namespace glTF