#pragma once

#include "float16_t/float16_t.hpp"
#include <cstdint>

static constexpr float uint8_max_as_float = 255.0F;

// 32-bit RGBA color representation
struct Color {
  float r;
  float g;
  float b;
  float a;

  Color() : r(0), g(0), b(0), a(1) {}

  // NOLINTNEXTLINE
  Color(float red, float green, float blue, float alpha = 1.0F)
      : r(red), g(green), b(blue), a(alpha) {}

  // NOLINTNEXTLINE
  Color(int red, int green, int blue, int alpha = 255)
      : r(static_cast<float>(red) / uint8_max_as_float),
        g(static_cast<float>(green) / uint8_max_as_float),
        b(static_cast<float>(blue) / uint8_max_as_float),
        a(static_cast<float>(alpha) / uint8_max_as_float) {}

  static constexpr uint32_t RedShift = 24U;
  static constexpr uint32_t GreenShift = 16U;
  static constexpr uint32_t BlueShift = 8U;
  static constexpr uint32_t BitMask = 0xFFU;

  // NOLINTNEXTLINE
  Color(uint32_t packedRGBA) {
    uint8_t red = (packedRGBA >> RedShift) & BitMask;
    uint8_t green = (packedRGBA >> GreenShift) & BitMask;
    uint8_t blue = (packedRGBA >> BlueShift) & BitMask;
    uint8_t alpha = packedRGBA & BitMask;
  };

  [[nodiscard]] auto Pack() const -> uint32_t {
    auto red = static_cast<float>(r) * uint8_max_as_float;
    auto green = static_cast<float>(g) * uint8_max_as_float;
    auto blue = static_cast<float>(b) * uint8_max_as_float;
    auto alpha = static_cast<float>(a) * uint8_max_as_float;

    return (static_cast<uint32_t>(red) << RedShift) |
           (static_cast<uint32_t>(green) << GreenShift) |
           (static_cast<uint32_t>(blue) << BlueShift) |
           static_cast<uint32_t>(alpha);
  }
};