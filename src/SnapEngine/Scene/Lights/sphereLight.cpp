#include "sphereLight.hpp"
#include "light.hpp"
#include <flecs.h>
#include <span>

namespace Engine::Scene {

auto SphereLight::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Base",
          .format = Light::GetBufferFormat(),
      },
      Graphics::BufferComponent{
          .name = "Range",
          .format = VK_FORMAT_R32_SFLOAT,
      },
      Graphics::BufferComponent{
          .name = "Radius",
          .format = VK_FORMAT_R32_SFLOAT,
      },
  });

  return format;
}

auto SphereLight::Write(std::span<uint8_t> buffer, flecs::entity lightEntity)
    -> Error {
  const auto &light = lightEntity.get<Light>();
  const auto &self = lightEntity.get<SphereLight>();

  auto &format = GetBufferFormat();
  auto offset = light.BufferIndex * format.GetStride();
  if (offset + format.GetStride() > buffer.size()) {
    return Error::Create("Writing light data out of bounds.");
  }

  auto newOffset = light.Write(buffer, offset);

  // NOLINTBEGIN
  auto *floatData = reinterpret_cast<float *>(buffer.data() + newOffset);
  floatData[0] = self.Range;
  floatData[1] = self.Radius;
  // NOLINTEND

  return {};
}

} // namespace Engine::Scene
