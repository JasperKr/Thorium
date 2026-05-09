#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/Math/vector.hpp"
#include "Scene/Lights/light.hpp"
#include <cstdint>
#include <flecs.h>

namespace Engine::Scene {

struct RectangleLight {
  Math::Vec2 Size{1.0F, 1.0F};
  float Range{};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  static auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity)
      -> Error;
};

constexpr size_t MaxRectangleLights = 32;

} // namespace Engine::Scene