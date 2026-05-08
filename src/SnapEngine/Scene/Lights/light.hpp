#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/color.hpp"
#include <cstdint>
namespace Engine::Scene {

enum class IntensityUnit : uint8_t {
  // Candela; (lm/sr) aka (luminous flux per steradian)
  LuminousIntensity,

  // Lux; (lm/m^2) aka (luminous flux per square meter)
  Illuminance,
};

struct Light {
  Color Color;

  IntensityUnit IntensityUnit = IntensityUnit::LuminousIntensity;
  float Intensity{};

  static Graphics::BufferFormat BufferFormat;
};

auto Light::BufferFormat = Graphics::BufferFormat({
    Graphics::BufferComponent{
        .name = "Color",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Intensity",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Position",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Rotation",
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
    },
});

} // namespace Engine::Scene