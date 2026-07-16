#include "light.hpp"
#include "Modules/console.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "renderer.hpp"
#include <cassert>

namespace Engine {

auto Light::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Color",
          .format = VK_FORMAT_R32G32B32_SFLOAT,
      },
      Graphics::BufferComponent{
          .name = "Intensity",
          .format = VK_FORMAT_R32_SFLOAT,
      },
      Graphics::BufferComponent{
          .name = "ShadowBufferIndex",
          .format = VK_FORMAT_R32_SINT,
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

  return format;
}

Light::~Light() {
  if (BufferIndex == -1) {
    return;
  }

  auto &buffers = Renderer::RendererInstance.GetSceneLightBuffers();

  switch (Type) {
  case LightType::None:
    PrintError("Destroying light with type None.");
    break;
  case LightType::Directional:
    assert(BufferIndex < MaxDirectionalLights);
    UsedDirectionalLightIndices.at(BufferIndex) = false;
    buffers.DirectionalLightCount--;
    break;
  case LightType::Point:
    assert(BufferIndex < MaxPointLights);
    UsedPointLightIndices.at(BufferIndex) = false;
    buffers.PointLightCount--;
    break;
  case LightType::Spot:
    assert(BufferIndex < MaxSpotLights);
    UsedSpotLightIndices.at(BufferIndex) = false;
    buffers.SpotLightCount--;
    break;
  case LightType::Rectangle:
    assert(BufferIndex < MaxRectangleLights);
    UsedRectangleLightIndices.at(BufferIndex) = false;
    buffers.RectangleLightCount--;
    break;
  case LightType::Sphere:
    assert(BufferIndex < MaxSphereLights);
    UsedSphereLightIndices.at(BufferIndex) = false;
    buffers.SphereLightCount--;
    break;
  }
}

} // namespace Engine