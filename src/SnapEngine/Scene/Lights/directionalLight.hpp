#pragma once

#include "Graphics/bufferformat.hpp"
#include "Scene/Lights/light.hpp"
namespace Engine::Scene {

constexpr size_t DirectionalLightShadowmapCascadeCount = 4;
constexpr size_t DirectionalLightShadowmapResolution = 2048;

struct DirectionalLight {
  static Graphics::BufferFormat BufferFormat;
};

constexpr size_t MaxDirectionalLights = 1;

auto DirectionalLight::BufferFormat = Graphics::BufferFormat({
    Graphics::BufferComponent{
        .name = "Base",
        .format = Light::BufferFormat,
    },
});

} // namespace Engine::Scene