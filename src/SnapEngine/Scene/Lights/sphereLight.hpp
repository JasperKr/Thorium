#pragma once

#include "Graphics/bufferformat.hpp"
#include "Scene/Lights/light.hpp"
#include <cstdint>
#include <flecs.h>
#include <span>
namespace Engine::Scene {

struct SphereLight {
  float Range{};
  float Radius{1.0F};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  static auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity)
      -> Error;
};

constexpr size_t MaxSphereLights = 32;

} // namespace Engine::Scene