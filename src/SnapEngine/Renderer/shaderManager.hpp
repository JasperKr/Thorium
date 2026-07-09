#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include <string_view>
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
};

struct ShaderConfiguration {
  std::string path;
  std::string name;
  std::vector<slang::PreprocessorMacroDesc> Externs;
};

const std::unordered_map<ShaderKey, ShaderConfiguration> ShaderConfigurations =
    {
        {ShaderKey::DepthPrepass,
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
         {.path =
              "Scripting/Graphics/Shaders/PostProcessing/fillSkybox.slang"}},
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
};

struct ShaderManager {
  auto GetShader(ShaderKey shaderKey) -> Result<Ref<Graphics::Shader>>;
  auto ReloadShaders() -> void;

private:
  std::unordered_map<ShaderKey, Ref<Graphics::Shader>> LoadedShaders;
};

} // namespace Engine::Renderer