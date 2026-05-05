#pragma once

#include "Modules/color.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <unordered_map>
namespace Engine::Style {

enum class UIStyles : uint8_t {
  Blueish,
  Greenish,
};

enum class Brightness : uint8_t {
  Bright,
  LightGray,
  DarkGray,
  Dark,
};

struct UIStyleDefinition {
  Color Primary;
  Color Secondary;
  Color Tertiary;
  Color Quaternary;
};

const std::unordered_map<UIStyles, UIStyleDefinition> UIStylePresets{
    {UIStyles::Blueish,
     {
         .Primary = Color(0x222831FF),
         .Secondary = Color(0x393E46FF),
         .Tertiary = Color(0x948979FF),
         .Quaternary = Color(0xDFD0B8FF),
     }},
    {UIStyles::Greenish,
     {
         .Primary = Color(0x2C3930FF),
         .Secondary = Color(0x3F4F44FF),
         .Tertiary = Color(0x52665AFF),
         .Quaternary = Color(0xDCD7C9FF),
     }},
};

auto ApplyDefaultStyle(UIStyles uiStyle) -> Error;
auto LuminanceFromColor(const Color &color) -> float;
auto ColorToBrightness(const Color &color) -> Brightness;
auto GetTextColorForBackground(const Color &background) -> Color;
auto Accent(const Color &color, float accentStrength = 1.0F) -> Color;
auto Saturate(const Color &color, float saturationFactor) -> Color;
auto Desaturate(const Color &color, float desaturationFactor) -> Color;

} // namespace Engine::Style