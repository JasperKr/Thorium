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
  float Intensity{};

  /// CPU only data ///
  IntensityUnit IntensityUnit = IntensityUnit::LuminousIntensity;
  uint32_t BufferIndex{};

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
};

} // namespace Engine::Scene