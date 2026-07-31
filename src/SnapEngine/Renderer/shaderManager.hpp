#pragma once

#include "Graphics/shader.hpp"
namespace Engine::Renderer {

enum class ShaderKey : uint8_t {
  DepthPrepass,
  DepthMaskPass,
  Deferred,
  SimpleLighting,
  PostProcessing,
  Skybox,
  FillSkybox,
  TransparencyForward,
  ApplyEnvironmentMap,
  DownsampleEnvironmentMap,
  EnvironmentMapToOctahedral,
  PrefilterRadiance,
  PrefilterIrradiance,
  StoreEnvironmentMap,
  ObjectPicker,

  Shadows_Directional,
  Shadows_Point,
  Shadows_Spot,
  Shadows_Sphere,
  Shadows_Rectangle,
  Shadows_Denoise,
  Shadows_ClearHitFlags,

  RTAO,
  AO_Denoise,
};

struct ShaderConfiguration {
  std::string path;
  std::string name;
  std::vector<slang::PreprocessorMacroDesc> externs;
};

const std::unordered_map<ShaderKey, ShaderConfiguration> ShaderConfigurations =
    {{ShaderKey::DepthPrepass,
      {.path = "Scripting/Graphics/Shaders/Geometry/depthPrepass.slang"}},
     {ShaderKey::DepthMaskPass,
      {.path = "Scripting/Graphics/Shaders/Geometry/depthMaskPass.slang"}},
     {ShaderKey::Deferred,
      {.path = "Scripting/Graphics/Shaders/Geometry/deferred.slang"}},
     {ShaderKey::SimpleLighting,
      {.path = "Scripting/Graphics/Shaders/Lighting/simple.slang"}},
     {ShaderKey::PostProcessing,
      {.path = "Scripting/Graphics/Shaders/PostProcessing/"
               "postProcessing.slang"}},
     {ShaderKey::Skybox,
      {.path = "Scripting/Graphics/Shaders/PostProcessing/skybox.slang"}},
     {ShaderKey::FillSkybox,
      {.path = "Scripting/Graphics/Shaders/PostProcessing/fillSkybox.slang"}},
     {ShaderKey::TransparencyForward,
      {.path =
           "Scripting/Graphics/Shaders/Geometry/transparencyForward.slang"}},
     {ShaderKey::ApplyEnvironmentMap,
      {.path = "Scripting/Graphics/Shaders/IBL/sampleEnvMaps.slang"}},
     {ShaderKey::DownsampleEnvironmentMap,
      {.path = "Scripting/Graphics/Shaders/IBL/downsample.slang"}},
     {ShaderKey::EnvironmentMapToOctahedral,
      {.path = "Scripting/Graphics/Shaders/IBL/envToOct.slang"}},
     {ShaderKey::PrefilterRadiance,
      {.path = "Scripting/Graphics/Shaders/IBL/filterRadiance.slang"}},
     {ShaderKey::PrefilterIrradiance,
      {.path = "Scripting/Graphics/Shaders/IBL/filterIrradiance.slang"}},
     {ShaderKey::StoreEnvironmentMap,
      {.path = "Scripting/Graphics/Shaders/IBL/storeEnvMap.slang"}},
     {ShaderKey::ObjectPicker,
      {.path = "Scripting/Graphics/Shaders/Editor/objectPicker.slang"}},
     {ShaderKey::Shadows_Directional,
      {.path = "Scripting/Graphics/Shaders/Lighting/Shadows/shadows.slang",
       .externs = {{"SHADER_LIGHT_TYPE_DIRECTIONAL", "1"}}}},
     {ShaderKey::Shadows_Point,
      {.path = "Scripting/Graphics/Shaders/Lighting/Shadows/shadows.slang",
       .externs = {{"SHADER_LIGHT_TYPE_POINT", "1"}}}},
     {ShaderKey::Shadows_Spot,
      {.path = "Scripting/Graphics/Shaders/Lighting/Shadows/shadows.slang",
       .externs = {{"SHADER_LIGHT_TYPE_SPOT", "1"}}}},
     {ShaderKey::Shadows_Sphere,
      {.path = "Scripting/Graphics/Shaders/Lighting/Shadows/shadows.slang",
       .externs = {{"SHADER_LIGHT_TYPE_SPHERE", "1"}}}},
     {ShaderKey::Shadows_Rectangle,
      {.path = "Scripting/Graphics/Shaders/Lighting/Shadows/shadows.slang",
       .externs = {{"SHADER_LIGHT_TYPE_RECTANGLE", "1"}}}},
     {ShaderKey::Shadows_Denoise,
      {.path = "Scripting/Graphics/Shaders/Lighting/Shadows/denoiser.slang"}},
     {ShaderKey::Shadows_ClearHitFlags,
      {.path = "Scripting/Graphics/Shaders/Lighting/Shadows/"
               "clearHitFlags.slang"}},
     {ShaderKey::RTAO,
      {.path = "Scripting/Graphics/Shaders/Lighting/AmbientOcclusion/"
               "rtao.slang"}},
     {ShaderKey::AO_Denoise,
      {.path = "Scripting/Graphics/Shaders/Lighting/AmbientOcclusion/"
               "denoiser.slang"}}};

struct ShaderManager {
  auto GetShader(ShaderKey shaderKey) -> Result<Ref<Graphics::Shader>>;
  auto ReloadShaders() -> void;

private:
  std::unordered_map<ShaderKey, Ref<Graphics::Shader>> LoadedShaders;
};

} // namespace Engine::Renderer