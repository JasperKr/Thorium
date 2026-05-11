#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/color.hpp"
#include <cstdint>
namespace Engine {

enum class IntensityUnit : uint8_t {
  // Candela; (lm/sr) aka (luminous flux per steradian)
  LuminousIntensity,

  // Lux; (lm/m^2) aka (luminous flux per square meter)
  Illuminance,
};

enum class LightType : uint8_t {
  None,
  Directional,
  Point,
  Spot,
  Rectangle,
  Sphere,
};

struct Light {
  Light() = default;
  Light(const Light &) = default;
  auto operator=(const Light &) -> Light & = default;
  Light(Light &&) = default;
  auto operator=(Light &&) -> Light & = default;
  ~Light();

  Color Color;
  float Intensity{};

  /// CPU only data ///
  IntensityUnit IntensityUnit = IntensityUnit::LuminousIntensity;
  int32_t BufferIndex = -1;
  LightType Type = LightType::None;

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  // NOLINTNEXTLINE
  auto Write(std::span<uint8_t> buffer, size_t offset) const -> size_t {
    assert(offset + (4UL * sizeof(float)) <= buffer.size());

    // NOLINTBEGIN
    auto *floatData = reinterpret_cast<float *>(buffer.data() + offset);
    floatData[0] = Color.r;
    floatData[1] = Color.g;
    floatData[2] = Color.b;
    floatData[3] = Intensity;
    // NOLINTEND

    return 4UL * sizeof(float);
  }

  auto SetColor(const struct Color &color) -> void { Color = color; }
  auto SetColor(float red, float green, float blue) -> void {
    Color.r = red;
    Color.g = green;
    Color.b = blue;
  }
  auto SetIntensity(float intensity) -> void { Intensity = intensity; }
  [[nodiscard]] auto GetColor() const -> struct Color {
    return Color;
  } [[nodiscard]] auto GetIntensity() const -> float {
    return Intensity;
  }
};

} // namespace Engine