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

  switch (Type) {
  case LightType::None:
    PrintError("Destroying light with type None.");
    break;
  case LightType::Directional:
    assert(BufferIndex < MaxDirectionalLights);
    UsedDirectionalLightIndices.at(BufferIndex) = false;
    Renderer::RendererInstance.SceneLightBuffers.DirectionalLightCount--;
    break;
  case LightType::Point:
    assert(BufferIndex < MaxPointLights);
    UsedPointLightIndices.at(BufferIndex) = false;
    Renderer::RendererInstance.SceneLightBuffers.PointLightCount--;
    break;
  case LightType::Spot:
    assert(BufferIndex < MaxSpotLights);
    UsedSpotLightIndices.at(BufferIndex) = false;
    Renderer::RendererInstance.SceneLightBuffers.SpotLightCount--;
    break;
  case LightType::Rectangle:
    assert(BufferIndex < MaxRectangleLights);
    UsedRectangleLightIndices.at(BufferIndex) = false;
    Renderer::RendererInstance.SceneLightBuffers.RectangleLightCount--;
    break;
  case LightType::Sphere:
    assert(BufferIndex < MaxSphereLights);
    UsedSphereLightIndices.at(BufferIndex) = false;
    Renderer::RendererInstance.SceneLightBuffers.SphereLightCount--;
    break;
  }
}

} // namespace Engine