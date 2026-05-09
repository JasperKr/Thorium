#include "rectangleLight.hpp"

namespace Engine::Scene {

auto RectangleLight::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Base",
          .format = Light::GetBufferFormat(),
      },
      Graphics::BufferComponent{
          .name = "Size",
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      Graphics::BufferComponent{
          .name = "Range",
          .format = VK_FORMAT_R32_SFLOAT,
      },
  });

  return format;
}

auto RectangleLight::Write(std::span<uint8_t> buffer, flecs::entity lightEntity)
    -> Error {
  const auto &light = lightEntity.get<Light>();
  const auto &self = lightEntity.get<RectangleLight>();

  auto &format = GetBufferFormat();
  auto offset = light.BufferIndex * format.GetStride();
  if (offset + format.GetStride() > buffer.size()) {
    return Error::Create("Writing light data out of bounds.");
  }

  auto newOffset = light.Write(buffer, offset);

  // NOLINTBEGIN
  auto *floatData = reinterpret_cast<float *>(buffer.data() + newOffset);
  floatData[0] = self.Size.x;
  floatData[1] = self.Size.y;
  floatData[2] = self.Range;
  // NOLINTEND

  return {};
}

} // namespace Engine::Scene