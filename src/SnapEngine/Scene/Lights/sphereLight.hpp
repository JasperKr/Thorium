#pragma once

#include "Graphics/bufferformat.hpp"
#include "Scene/Lights/light.hpp"
namespace Engine::Scene {

struct SphereLight {
  float Range{};
  float Radius{1.0F};

  static Graphics::BufferFormat BufferFormat;
};

constexpr size_t MaxSphereLights = 32;

auto SphereLight::BufferFormat = Graphics::BufferFormat({
    Graphics::BufferComponent{
        .name = "Base",
        .format = Light::BufferFormat,
    },
    Graphics::BufferComponent{
        .name = "Range",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Radius",
        .format = VK_FORMAT_R32_SFLOAT,
    },
});

} // namespace Engine::Scene