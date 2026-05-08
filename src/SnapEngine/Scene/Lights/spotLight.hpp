#pragma once

#include "Graphics/bufferformat.hpp"
#include "Scene/Lights/light.hpp"
namespace Engine::Scene {

struct SpotLight {
  float Range{};
  float InnerConeAngle{};
  float OuterConeAngle{};

  static Graphics::BufferFormat BufferFormat;
};

constexpr size_t MaxSpotLights = 32;

auto SpotLight::BufferFormat = Graphics::BufferFormat({
    Graphics::BufferComponent{
        .name = "Base",
        .format = Light::BufferFormat,
    },
    Graphics::BufferComponent{
        .name = "Range",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "InnerConeAngle",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "OuterConeAngle",
        .format = VK_FORMAT_R32_SFLOAT,
    },
});

} // namespace Engine::Scene