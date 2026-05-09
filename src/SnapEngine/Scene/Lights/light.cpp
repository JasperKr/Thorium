#include "light.hpp"

namespace Engine::Scene {

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

} // namespace Engine::Scene