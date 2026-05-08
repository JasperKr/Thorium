#pragma once

#include "Graphics/bufferformat.hpp"
#include "Scene/Lights/light.hpp"
namespace Engine::Scene {

struct PointLight {
  float Range{};

  static Graphics::BufferFormat BufferFormat;
};

constexpr size_t MaxPointLights = 512;

auto PointLight::BufferFormat = Graphics::BufferFormat({
    Graphics::BufferComponent{
        .name = "Base",
        .format = Light::BufferFormat,
    },
    Graphics::BufferComponent{
        .name = "Range",
        .format = VK_FORMAT_R32_SFLOAT,
    },
});

} // namespace Engine::Scene