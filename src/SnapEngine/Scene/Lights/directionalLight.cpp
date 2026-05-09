#include "directionalLight.hpp"

namespace Engine::Scene {

auto DirectionalLight::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Base",
          .format = Light::GetBufferFormat(),
      },
  });

  return format;
}

auto DirectionalLight::Write(std::span<uint8_t> buffer,
                             flecs::entity lightEntity) -> Error {
  const auto &light = lightEntity.get<Light>();
  const auto &self = lightEntity.get<DirectionalLight>();
  auto &format = GetBufferFormat();

  auto offset = light.BufferIndex * format.GetStride();

  if (offset + format.GetStride() > buffer.size()) {
    return Error::Create("Writing light data out of bounds.");
  }

  light.Write(buffer, offset);

  return {};
}

} // namespace Engine::Scene