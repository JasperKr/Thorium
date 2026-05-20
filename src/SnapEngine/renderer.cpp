#include "renderer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/color.hpp"
#include "Modules/error.hpp"
#include "Modules/imagedata.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "material.hpp"
#include <cassert>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Renderer RendererInstance{};

auto Renderer::InitializeDefaultMaterial(Graphics::GraphicsContext &context)
    -> Error {
  constexpr uint32_t CheckerboardTextureSize = 8;
  constexpr Color CheckerboardColorPrimary = {0.0F, 0.0F, 0.0F, 1.0F};
  constexpr Color CheckerboardColorSecondary = {1.0F, 0.0F, 1.0F, 1.0F};

  Image::ImageData defaultTextureData(CheckerboardTextureSize,
                                      CheckerboardTextureSize,
                                      VK_FORMAT_R8G8B8A8_UNORM);

  for (uint32_t column = 0; column < CheckerboardTextureSize; ++column) {
    for (uint32_t row = 0; row < CheckerboardTextureSize; ++row) {
      bool isPrimaryColor = (row % 2 == 0 && column % 2 == 0) ||
                            (row % 2 == 1 && column % 2 == 1);

      Color pixelColor = isPrimaryColor ? CheckerboardColorPrimary
                                        : CheckerboardColorSecondary;

      CHECK_ERR(defaultTextureData.SetColor({column, row}, pixelColor));
    }
  }

  auto defaultTextureResult = Graphics::LoadFromMemory(
      context, defaultTextureData, VK_IMAGE_USAGE_SAMPLED_BIT);

  if (Error::IsError(defaultTextureResult)) {
    return defaultTextureResult.error();
  }

  Image::ImageData defaultNormalTextureData(1, 1, VK_FORMAT_R8G8B8A8_UNORM);

  const Color pixelColor = {0.5F, 0.5F, 1.0F, 1.0F};
  CHECK_ERR(defaultNormalTextureData.SetColor({0, 0}, pixelColor));

  auto defaultNormalTextureResult = Graphics::LoadFromMemory(
      context, defaultNormalTextureData, VK_IMAGE_USAGE_SAMPLED_BIT);

  if (Error::IsError(defaultNormalTextureResult)) {
    return defaultNormalTextureResult.error();
  }
  auto defaultNormalTexture = defaultNormalTextureResult.value();

  Image::ImageData defaultBlackTextureData(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
  const Color blackColor = {0.0F, 0.0F, 0.0F, 1.0F};
  CHECK_ERR(defaultBlackTextureData.SetColor({0, 0}, blackColor));

  auto defaultBlackTextureResult = Graphics::LoadFromMemory(
      context, defaultBlackTextureData, VK_IMAGE_USAGE_SAMPLED_BIT);
  if (Error::IsError(defaultBlackTextureResult)) {
    return defaultBlackTextureResult.error();
  }
  auto defaultBlackTexture = defaultBlackTextureResult.value();

  auto defaultWhiteTextureResult = Graphics::GetDefaultTexture(
      context, VK_FORMAT_R8G8B8A8_UNORM, Graphics::TextureType::DEFAULT);
  if (Error::IsError(defaultWhiteTextureResult)) {
    return defaultWhiteTextureResult.error();
  }
  auto defaultWhiteTexture = defaultWhiteTextureResult.value();

  auto material = Material();

  material.name = "No Material";
  material.albedoTexture = defaultTextureResult.value();
  material.albedoTexture->SetFilter(VK_FILTER_LINEAR, VK_FILTER_NEAREST,
                                    VK_SAMPLER_MIPMAP_MODE_NEAREST);
  material.metallicRoughnessTexture = material.albedoTexture;
  material.emissiveTexture = defaultWhiteTexture;
  material.normalTexture = defaultNormalTexture;
  material.reflectanceTexture = defaultWhiteTexture;

  material.cullMode = VK_CULL_MODE_NONE;
  material.alphaMode = AlphaMode::Opaque;

  NoMaterial = material;

  material = Material();
  material.name = "Default Material";
  material.albedoTexture = defaultWhiteTexture;
  material.metallicRoughnessTexture = defaultWhiteTexture;
  material.emissiveTexture = defaultWhiteTexture;
  material.normalTexture = defaultNormalTexture;
  material.reflectanceTexture = defaultWhiteTexture;

  material.cullMode = VK_CULL_MODE_BACK_BIT;
  material.alphaMode = AlphaMode::Opaque;

  DefaultMaterial = material;

  return {};
}

auto Renderer::ResizeMaterialBuffer(Graphics::GraphicsContext &context,
                                    size_t newSize) -> Error {
  auto previousSize = MaterialsBuffer->GetElementCount();
  if (newSize <= previousSize) {
    return {};
  }

  auto format = Graphics::BufferFormat(MaterialBufferComponents);

  auto newBufferResult = Graphics::StructuredBuffer::Create(
      *Graphics::GetCurrentGraphicsContext(), format, newSize,
      Graphics::StructuredBufferCreationInfo{
          .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          .debugName = "Material Buffer",
      });

  if (Error::IsError(newBufferResult)) {
    return newBufferResult.error();
  }

  auto newBuffer = newBufferResult.value();

  auto copyError = MaterialsBuffer->GetBuffer()->CopyTo(
      context, *newBuffer->GetBuffer(), 0, 0, MaterialsBuffer->GetSize());
  if (Error::IsError(copyError)) {
    return copyError;
  }

  MaterialsBuffer = std::move(newBuffer);

  return {};
}

auto Renderer::GetNewMaterialIndex() -> Result<size_t> {
  size_t newIndex = 0;
  while (UsedMaterialIndices.contains(newIndex)) {
    newIndex++;
  }
  UsedMaterialIndices.insert(newIndex);

  if (newIndex >= MaterialsBuffer->GetElementCount()) {
    auto newSize = MaterialsBuffer->GetElementCount() * 2;

    auto resizeResult =
        ResizeMaterialBuffer(*Graphics::GetCurrentGraphicsContext(), newSize);
    if (Error::IsError(resizeResult)) {
      return resizeResult;
    }
  }

  return newIndex;
}

auto Renderer::InitializeMaterialBuffer(Graphics::GraphicsContext &context,
                                        size_t initialSize) -> Error {
  const Graphics::StructuredBufferCreationInfo materialBufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags =
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Material Buffer",
  };

  auto format = Graphics::BufferFormat(MaterialBufferComponents);

  auto materialBufferResult = Graphics::StructuredBuffer::Create(
      context, format, initialSize, materialBufferCreateInfo);

  if (Error::IsError(materialBufferResult)) {
    return materialBufferResult.error();
  }

  MaterialsBuffer = materialBufferResult.value();

  return {};
}

auto Renderer::InitializeLightBuffers(Graphics::GraphicsContext &context)
    -> Error {
  Graphics::StructuredBufferCreationInfo bufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags =
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Directional Light Buffer",
  };

  auto bufferResult = Graphics::StructuredBuffer::Create(
      context, DirectionalLight::GetBufferFormat(), MaxDirectionalLights,
      bufferCreateInfo);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }
  SceneLightBuffers.DirectionalLightsBuffer = bufferResult.value();

  bufferCreateInfo.debugName = "Point Light Buffer";
  bufferResult = Graphics::StructuredBuffer::Create(
      context, PointLight::GetBufferFormat(), MaxPointLights, bufferCreateInfo);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }
  SceneLightBuffers.PointLightsBuffer = bufferResult.value();

  bufferCreateInfo.debugName = "Spot Light Buffer";
  bufferResult = Graphics::StructuredBuffer::Create(
      context, SpotLight::GetBufferFormat(), MaxSpotLights, bufferCreateInfo);
  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }
  SceneLightBuffers.SpotLightsBuffer = bufferResult.value();

  bufferCreateInfo.debugName = "Rectangle Light Buffer";
  bufferResult = Graphics::StructuredBuffer::Create(
      context, RectangleLight::GetBufferFormat(), MaxRectangleLights,
      bufferCreateInfo);
  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }
  SceneLightBuffers.RectangleLightsBuffer = bufferResult.value();

  bufferCreateInfo.debugName = "Sphere Light Buffer";
  bufferResult = Graphics::StructuredBuffer::Create(
      context, SphereLight::GetBufferFormat(), MaxSphereLights,
      bufferCreateInfo);
  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }
  SceneLightBuffers.SphereLightsBuffer = bufferResult.value();

  SceneLightBuffers.DirectionalLightData.resize(
      MaxDirectionalLights *
      SceneLightBuffers.DirectionalLightsBuffer->GetStride());
  SceneLightBuffers.PointLightData.resize(
      MaxPointLights * SceneLightBuffers.PointLightsBuffer->GetStride());
  SceneLightBuffers.SpotLightData.resize(
      MaxSpotLights * SceneLightBuffers.SpotLightsBuffer->GetStride());
  SceneLightBuffers.RectangleLightData.resize(
      MaxRectangleLights *
      SceneLightBuffers.RectangleLightsBuffer->GetStride());
  SceneLightBuffers.SphereLightData.resize(
      MaxSphereLights * SceneLightBuffers.SphereLightsBuffer->GetStride());

  return {};
}

auto Renderer::BindLightBuffers(
    const Graphics::GraphicsContext &context,
    const Ref<Graphics::Shader::ShaderModule> &shader) const -> Error {
  using Key = Graphics::ResourceKey;

  static auto key = Key{"PointLights"};
  // auto error = shader->Send(context, key, SceneLightBuffers.PointLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  // key = Key{"SpotLights"};
  // error = shader->Send(context, key, SceneLightBuffers.SpotLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  // key = Key{"RectangleLights"};
  // error = shader->Send(context, key, SceneLightBuffers.RectangleLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  // key = Key{"SphereLights"};
  // error = shader->Send(context, key, SceneLightBuffers.SphereLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  key = Key{"DirectionalLights"};
  auto error =
      shader->Send(context, key, SceneLightBuffers.DirectionalLightsBuffer);
  if (Error::IsError(error)) {
    return error;
  }

  return {};
}

auto Renderer::GetShader(ShaderKey shaderKey)
    -> Result<Ref<Graphics::Shader::ShaderModule>> {
  auto iterator = LoadedShaders.find(shaderKey);
  if (iterator != LoadedShaders.end()) {
    return iterator->second;
  }

  auto configurationIter = ShaderConfigurations.find(shaderKey);
  if (configurationIter == ShaderConfigurations.end()) {
    auto intKey = static_cast<int>(shaderKey);
    return Error::Unexpectedf("Shader not found for key: {}", intKey);
  }

  const auto &configuration = configurationIter->second;

  auto context = *Graphics::GetCurrentGraphicsContext();

  auto moduleResult = Graphics::Shader::ShaderModule::Create(
      context, configuration.path, configuration.name);

  if (Error::IsError(moduleResult)) {
    return moduleResult.error();
  }

  auto shaderModule = moduleResult.value();
  LoadedShaders[shaderKey] = shaderModule;

  return shaderModule;
}

auto DrawFullScreen(const Graphics::GraphicsContext &context) -> Error {
  return Graphics::Draw(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                        3, // NOLINT
                        1);
}

} // namespace Engine::Renderer