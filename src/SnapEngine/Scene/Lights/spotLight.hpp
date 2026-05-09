#pragma once

#include "Graphics/bufferformat.hpp"
#include <cstdint>
#include <flecs.h>
#include <span>
namespace Engine::Scene {

struct SpotLight {
  float Range{};
  float InnerConeAngle{};
  float OuterConeAngle{};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  static auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity)
      -> Error;
};

constexpr size_t MaxSpotLights = 32;

} // namespace Engine::Scene