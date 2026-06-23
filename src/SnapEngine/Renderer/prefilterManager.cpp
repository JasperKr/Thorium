#include "prefilterManager.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Renderer/lightProbe.hpp"
#include "Scene/camera.hpp"
#include "Scene/cameraMatrices.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include <array>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

auto LightprobePrefilterManager::Initialize(
    const Graphics::GraphicsContext &context) -> Error {
  PrefilteredRadianceCoeffs =
      CHECK_RES(Filesystem::ReadFile("Graphics/Assets/coeffs_quad_32.bin"));

  PrefilteredRadianceCoeffsBuffer = CHECK_RES(Graphics::Buffer::Create(
      context,
      {
          .size = PrefilteredRadianceCoeffs.size(),
          .usage = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          .properties =
              static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
          .debugName = "Prefiltered Radiance Coefficients Buffer",
      }));

  CHECK_ERR(PrefilteredRadianceCoeffsBuffer->SetData(
      context, PrefilteredRadianceCoeffs));

  SceneCubemap = CHECK_RES(Graphics::Texture::Create(
      context, {
                   .size = {PrefilteredRadianceCubeMapSize,
                            PrefilteredRadianceCubeMapSize, 1},
                   .arrayLayers = 6,
                   .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                   .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                            VK_IMAGE_USAGE_STORAGE_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                   .mipmapCount = ProbeReflectionMipLevels,
                   .debugName = "Scene Cubemap",
                   .textureType = Graphics::TextureType::CUBEMAP,
               }));
  SceneCubemap->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                          VK_SAMPLER_MIPMAP_MODE_LINEAR);

  TemporaryRadianceMap = CHECK_RES(Graphics::Texture::Create(
      context, {
                   .size = {PrefilteredRadianceCubeMapSize,
                            PrefilteredRadianceCubeMapSize, 1},
                   .arrayLayers = 6,
                   .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                   .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                            VK_IMAGE_USAGE_STORAGE_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                   .mipmapCount = ProbeReflectionMipLevels,
                   .debugName = "Temporary Radiance Map",
                   .textureType = Graphics::TextureType::CUBEMAP,
               }));

  TemporaryRadianceMap->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                                  VK_SAMPLER_MIPMAP_MODE_LINEAR);

  constexpr static float EnvironmentMapLevel0PB = 6.0F;
  constexpr static float EnvironmentMapLevel1PB = 0.0F;

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    uint32_t levelSize = PrefilteredRadianceCubeMapSize >> level;

    float glossiness = (EnvironmentMapLevel0PB - static_cast<float>(level)) /
                       (EnvironmentMapLevel0PB - EnvironmentMapLevel1PB);

    levelParameters.at(level) = {
        .Level = level,
        .Glossiness = glossiness,
        .Roughness = GlossinessToRoughness(glossiness),
        .Resolution = levelSize,
    };
  }

  CHECK_ERR(CreateStorageTextures(context, EnvMapReallocationStep));

  DownsampleShader = CHECK_RES(Graphics::Shader::ShaderModule::Create(
      context, "Scripting/Graphics/Shaders/IBL/downsample.slang",
      "Environment map downsample"));

  EnvironmentMapToOctahedralShader =
      CHECK_RES(Graphics::Shader::ShaderModule::Create(
          context, "Scripting/Graphics/Shaders/IBL/envToOct.slang",
          "Store environment map"));

  PrefilterRadianceShader = CHECK_RES(Graphics::Shader::ShaderModule::Create(
      context, "Scripting/Graphics/Shaders/IBL/filterRadiance.slang",
      "Prefilter radiance"));

  PrefilterIrradianceShader = CHECK_RES(Graphics::Shader::ShaderModule::Create(
      context, "Scripting/Graphics/Shaders/IBL/filterIrradiance.slang",
      "Prefilter irradiance"));

  StoreEnvironmentMapShader = CHECK_RES(Graphics::Shader::ShaderModule::Create(
      context, "Scripting/Graphics/Shaders/IBL/storeEnvMap.slang",
      "Store environment map"));

  // NOLINTNEXTLINE
  Camera = CHECK_RES(Engine::Camera::Create(
      context, 90.0F,
      {PrefilteredRadianceCubeMapSize, PrefilteredRadianceCubeMapSize}, 0.1F,
      100.0F));
  Camera.SetPersistentTextureSettings({
      .IncomingLight = true,
  });
  Camera.SetSettings(Camera::Settings{
      .DoPostProcessing = false,
  });

  return {};
}

auto LightprobePrefilterManager::CreateStorageTextures(
    const Graphics::GraphicsContext &context, uint32_t arrayLayers) -> Error {
  RadianceMapViews.clear();
  IrradianceMapViews.clear();

  RadianceMaps = CHECK_RES(Graphics::Texture::Create(
      context,
      Graphics::TextureCreationInfo{
          .size = {PrefilteredRadianceMapsSize, PrefilteredRadianceMapsSize, 1},
          .arrayLayers = arrayLayers,
          .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
          .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                   VK_IMAGE_USAGE_STORAGE_BIT,
          .mipmapCount = ProbeReflectionMipLevels,
          .debugName = "Radiance Maps",
          .textureType = Graphics::TextureType::ARRAY,
      }));

  for (uint32_t level = 0; level < RadianceMaps->GetMipmapCount(); level++) {
    auto view = CHECK_RES(Graphics::Texture::Create(
        context, RadianceMaps.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = arrayLayers,
        }));
    RadianceMapViews.emplace_back(view);
  }

  IrradianceMaps = CHECK_RES(Graphics::Texture::Create(
      context, Graphics::TextureCreationInfo{
                   .size = {PrefilteredIrradianceMapsSize,
                            PrefilteredIrradianceMapsSize, 1},
                   .arrayLayers = arrayLayers,
                   .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                   .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                            VK_IMAGE_USAGE_STORAGE_BIT,
                   .mipmapCount = 1,
                   .debugName = "Irradiance Maps",
                   .textureType = Graphics::TextureType::ARRAY,
               }));

  for (uint32_t level = 0; level < IrradianceMaps->GetMipmapCount(); level++) {
    auto view = CHECK_RES(Graphics::Texture::Create(
        context, IrradianceMaps.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = arrayLayers,
        }));
    IrradianceMapViews.emplace_back(view);
  }

  return {};
}

// NOLINTNEXTLINE
auto LightprobePrefilterManager::PrefilterRadianceMap(
    const Graphics::GraphicsContext &context,
    const Ref<Graphics::Texture> &envMap, const Ref<Graphics::Texture> &output,
    const LightProbe &lightProbe) -> Error {

  if (envMap->GetFormat() != VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
    return Error::Create("Environment map must be in rg11b10f format "
                         "for prefiltering.");
  }

  if (envMap->GetWidth() != PrefilteredRadianceCubeMapSize ||
      envMap->GetHeight() != PrefilteredRadianceCubeMapSize) {
    return Error::Createf("Environment map must be {}x{} for prefiltering.",
                          PrefilteredRadianceCubeMapSize,
                          PrefilteredRadianceCubeMapSize);
  }

  if (!envMap->SupportsSampling() || !envMap->SupportsStorage()) {
    return Error::Create("Environment map must support sampling and storage "
                         "for prefiltering.");
  }

  envMap->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                    VK_SAMPLER_MIPMAP_MODE_LINEAR);
  envMap->SetAnisotropy(0.0F); // NOLINT
  output->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                    VK_SAMPLER_MIPMAP_MODE_LINEAR);
  output->SetAnisotropy(0.0F); // NOLINT

  /// Prepare texture views ///

  std::vector<Ref<Graphics::Texture>> envMapCubeViews;
  std::vector<Ref<Graphics::Texture>> envMapArrayViews;
  std::vector<Ref<Graphics::Texture>> outputViews;

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    auto inputCubeView = CHECK_RES(Graphics::Texture::Create(
        context, envMap.get(), VK_IMAGE_VIEW_TYPE_CUBE,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }));
    envMapCubeViews.emplace_back(inputCubeView);

    auto inputArrayView = CHECK_RES(Graphics::Texture::Create(
        context, envMap.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6,
        }));
    envMapArrayViews.emplace_back(inputArrayView);

    auto outputView = CHECK_RES(Graphics::Texture::Create(
        context, output.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }));
    outputViews.emplace_back(outputView);
  }

  /// Run the downsample shader to generate the mipmaps for the environment map ///

  Graphics::DynamicRendering::SetShader(DownsampleShader);

  for (uint32_t level = 0; level < ProbeReflectionMipLevels - 1; level++) {
    auto &params = levelParameters.at(level);

    CHECK_ERR(
        DownsampleShader->Send({"tex_hi_res"}, envMapCubeViews.at(level)));
    CHECK_ERR(
        DownsampleShader->Send({"tex_lo_res"}, envMapArrayViews.at(level + 1)));

    CHECK_ERR(Graphics::DispatchWithin(
        context, {params.Resolution, params.Resolution, 6}));
  }

  // Prefilter the environment map for radiance ///

  CHECK_ERR(PrefilterRadianceShader->Send({"Coefficients"},
                                          PrefilteredRadianceCoeffsBuffer));
  CHECK_ERR(PrefilterRadianceShader->Send({"InputTexture"}, envMap));

  Graphics::DynamicRendering::SetShader(PrefilterRadianceShader);

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    auto key = std::format("OutputTexture{}", level);
    const auto &view = outputViews.at(level);

    CHECK_ERR(PrefilterRadianceShader->Send({key.c_str()}, view));
  }

  constexpr static uint64_t PixelCount = Image::GetTexelCount(
      VkExtent2D{
          .width = PrefilteredRadianceCubeMapSize,
          .height = PrefilteredRadianceCubeMapSize,
      },
      ProbeReflectionMipLevels);

  CHECK_ERR(Graphics::DispatchWithin(context, {PixelCount, 6, 1}));

  /// Store the environment map in octahedral format for sampling in the shader ///

  CHECK_ERR(StoreEnvironmentMapShader->Send({"Input"}, TemporaryRadianceMap));

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    auto key = std::format("Output_Mip_{}", level);
    const auto &view = RadianceMapViews.at(level);

    CHECK_ERR(StoreEnvironmentMapShader->Send({key.c_str()}, view));
  }

  CHECK_ERR(Graphics::Shader::UniformWriter::Send(
      StoreEnvironmentMapShader, {"Slice"}, lightProbe.EnvironmentMapIndex));

  Graphics::DynamicRendering::SetShader(StoreEnvironmentMapShader);

  constexpr static uint64_t FullPixelCount = Image::GetTexelCount(
      VkExtent2D{
          .width = PrefilteredRadianceMapsSize,
          .height = PrefilteredRadianceMapsSize,
      },
      ProbeReflectionMipLevels);

  CHECK_ERR(Graphics::DispatchWithin(context, {FullPixelCount, 1, 1}));

  return {};
}

auto LightprobePrefilterManager::PrefilterIrradianceMap(
    const Graphics::GraphicsContext &context, int32_t slice) -> Error {
  if (slice < 0) {
    return Error::Create("Invalid environment map index");
  }

  CHECK_ERR(Graphics::Shader::UniformWriter::Send(PrefilterIrradianceShader,
                                                  {"Slice"}, slice));
  CHECK_ERR(PrefilterIrradianceShader->Send({"RadianceInput"}, RadianceMaps));
  CHECK_ERR(
      PrefilterIrradianceShader->Send({"IrradianceOutput"}, IrradianceMaps));

  // NOLINTBEGIN
  if (IrradianceMaps->GetWidth() != 32UL ||
      IrradianceMaps->GetHeight() != 32UL) {
    return Error::Create("Irradiance map must be 32x32 for prefiltering.");
  }
  // NOLINTEND

  Graphics::DynamicRendering::SetShader(PrefilterIrradianceShader);

  CHECK_ERR(
      Graphics::DispatchWithin(context, {PrefilteredIrradianceMapsSize,
                                         PrefilteredIrradianceMapsSize, 1}));

  return {};
}

auto LightprobePrefilterManager::PrefilterEnvironmentMap(
    const Graphics::GraphicsContext &context,
    const Ref<Graphics::Texture> &envMap, LightProbe &lightProbe) -> Error {
  if (lightProbe.EnvironmentMapIndex < 0) {
    return Error::Create("Invalid environment map index");
  }

  if (static_cast<uint32_t>(lightProbe.EnvironmentMapIndex) >= MaxEnvMaps) {
    return Error::Create("Environment map index out of bounds");
  }

  if (!EnvMapUsed.at(lightProbe.EnvironmentMapIndex)) {
    return Error::Create("Environment map index not in use");
  }

  CHECK_ERR(
      PrefilterRadianceMap(context, envMap, TemporaryRadianceMap, lightProbe));

  CHECK_ERR(PrefilterIrradianceMap(context, lightProbe.EnvironmentMapIndex));

  return {};
}

const static std::array<Engine::Transform, 6> Transforms = {
    Engine::Transform(Math::Vec3{0.0F, 0.0F, 0.0F}, // X+
                      Math::Conversions::ToQuaternion(
                          Math::EulerAngle(90.0F, 0.0F, 0.0F).ToRadians())),
    Engine::Transform(Math::Vec3{0.0F, 0.0F, 0.0F}, // X-
                      Math::Conversions::ToQuaternion(
                          Math::EulerAngle(-90.0F, 0.0F, 0.0F).ToRadians())),
    Engine::Transform(Math::Vec3{0.0F, 0.0F, 0.0F}, // Y+
                      Math::Conversions::ToQuaternion(
                          Math::EulerAngle(0.0F, 90.0F, 0.0F).ToRadians())),
    Engine::Transform(Math::Vec3{0.0F, 0.0F, 0.0F}, // Y-
                      Math::Conversions::ToQuaternion(
                          Math::EulerAngle(0.0F, -90.0F, 0.0F).ToRadians())),
    Engine::Transform(Math::Vec3{0.0F, 0.0F, 0.0F}, // Z+
                      Math::Conversions::ToQuaternion(
                          Math::EulerAngle(0.0F, 0.0F, 0.0F).ToRadians())),
    Engine::Transform(Math::Vec3{0.0F, 0.0F, 0.0F}, // Z-
                      Math::Conversions::ToQuaternion(
                          Math::EulerAngle(0.0F, 180.0F, 0.0F).ToRadians())),
};

const static Math::Matrix4x4 ProjectionMatrix =
    Math::Matrix4x4::Perspective(Math::DegToRad(90.0F), 1.0F, 0.1F, 100.0F);

const static Engine::CameraMatrices CameraMatrices{
    .ViewMatrix = Math::Matrix4x4::Identity(),
    .InverseViewMatrix = Math::Matrix4x4::Identity(),
    .ProjectionMatrix = ProjectionMatrix,
    .InverseProjectionMatrix = ProjectionMatrix.InverseTranspose(),
    .ViewProjectionMatrix = ProjectionMatrix,
    .InverseViewProjectionMatrix = ProjectionMatrix.InverseTranspose(),
    .RotationProjectionMatrix = ProjectionMatrix,
    .InverseRotationProjectionMatrix = ProjectionMatrix.InverseTranspose(),
};

auto LightprobePrefilterManager::PrefilterLightProbe(
    const Graphics::GraphicsContext &context, LightProbe &lightProbe,
    Engine::Scene *scene, const Transform &transform) -> Error {

  if (lightProbe.EnvironmentMapIndex < 0) {
    lightProbe.EnvironmentMapIndex = GetFreeEnvMapIndex().value_or(-1);
    PrintAlways("Allocated environment map index {} for light probe.",
                lightProbe.EnvironmentMapIndex);
  }

  std::vector<Ref<Graphics::Texture>> envMapArrayViews;

  for (uint32_t layer = 0; layer < 6; layer++) { // NOLINT
    auto inputArrayView = CHECK_RES(Graphics::Texture::Create(
        context, SceneCubemap.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = layer,
            .layerCount = 1,
        }));
    envMapArrayViews.emplace_back(inputArrayView);
  }

  for (int i = 0; i < Transforms.size(); i++) {
    auto cubemapTransfrom = Transforms.at(i);
    cubemapTransfrom.SetPosition(transform.GetPosition());
    cubemapTransfrom.UpdateLocalMatrix();
    cubemapTransfrom.UpdateWorldMatrix(nullptr);

    Engine::CameraMatrices drawMatrices = CameraMatrices;

    const auto &worldMatrix = cubemapTransfrom.GetWorldMatrix();

    drawMatrices.ViewMatrix = worldMatrix.InverseTranspose();
    drawMatrices.InverseViewMatrix = worldMatrix;
    drawMatrices.RotationMatrix = drawMatrices.ViewMatrix.AsMatrix3x3();
    drawMatrices.InverseRotationMatrix =
        drawMatrices.RotationMatrix.Transpose();
    drawMatrices.Update();

    CHECK_ERR(Camera.WriteToBuffer(drawMatrices, cubemapTransfrom));

    Engine::DrawData drawData{
        .Transform = cubemapTransfrom,
        .Matrices = drawMatrices,
    };

    CHECK_ERR(Camera.Render(context, drawData, scene));

    Graphics::DynamicRendering::SetShader({});

    CHECK_ERR(Graphics::DynamicRendering::SetRenderTargets(
        context, {
                     Graphics::DynamicRendering::RenderTarget{
                         .blendMode = Graphics::BlendmodeNone,
                         .texture = envMapArrayViews.at(i),
                         .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                     },
                 }));

    CHECK_ERR(Graphics::Draw(context,
                             *Camera.GetOwnedTextures().IncomingLight.get()));
  }

  CHECK_ERR(PrefilterEnvironmentMap(context, SceneCubemap, lightProbe));

  return {};
}

auto LightprobePrefilterManager::Deinitialize() -> void {
  RadianceMaps.reset();
  RadianceMapViews.clear();

  IrradianceMaps.reset();
  IrradianceMapViews.clear();

  SceneCubemap.reset();
  Camera = Engine::Camera();

  TemporaryRadianceMap.reset();

  PrefilteredRadianceCoeffs.clear();
  PrefilteredRadianceCoeffsBuffer.reset();

  EnvMapUsed.fill(false);

  DownsampleShader.reset();
  PrefilterRadianceShader.reset();
  PrefilterIrradianceShader.reset();
  StoreEnvironmentMapShader.reset();
  EnvironmentMapToOctahedralShader.reset();
}

} // namespace Engine::Renderer