#pragma once

#include "Graphics/bufferformat.hpp"
#include "Scene/Lights/light.hpp"
#include <cstdint>
#include <flecs.h>
#include <span>
namespace Engine::Scene {

constexpr size_t DirectionalLightShadowmapCascadeCount = 4;
constexpr size_t DirectionalLightShadowmapResolution = 2048;

struct DirectionalLight {
  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  static auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity)
      -> Error;
};

constexpr size_t MaxDirectionalLights = 1;

} // namespace Engine::Scene