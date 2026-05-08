#pragma once

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
};

} // namespace Engine::Scene