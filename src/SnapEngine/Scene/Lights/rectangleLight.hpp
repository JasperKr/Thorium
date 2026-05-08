#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/Math/vector.hpp"
#include "Scene/Lights/light.hpp"
namespace Engine::Scene {

struct RectangleLight {
  Math::Vec2 Size{1.0F, 1.0F};
  float Range{};

  static Graphics::BufferFormat BufferFormat;
};

constexpr size_t MaxRectangleLights = 32;

auto RectangleLight::BufferFormat = Graphics::BufferFormat({
    Graphics::BufferComponent{
        .name = "Base",
        .format = Light::BufferFormat,
    },
    Graphics::BufferComponent{
        .name = "Size",
        .format = VK_FORMAT_R32G32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Range",
        .format = VK_FORMAT_R32_SFLOAT,
    },
});

} // namespace Engine::Scene