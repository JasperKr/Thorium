#include "uniformWriter.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/shader.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include <array>
#include <cstdint>

namespace Graphics::Shader {
auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Ref<Buffer> &buffer) -> Error {
  return shader->Send(context, key, buffer);
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Ref<Graphics::Texture> &texture) -> Error {
  return shader->Send(context, key, texture);
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const std::span<const uint8_t> &data) -> Error {
  return shader->Send(context, key, data);
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         float value) -> Error {
  return shader->Send(context, key, SpanGenericType<float>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         int value) -> Error {
  return shader->Send(context, key, SpanGenericType<int>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         uint32_t value) -> Error {
  return shader->Send(context, key, SpanGenericType<uint32_t>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         bool value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<uint32_t>::GetSpan(
                          static_cast<const unsigned int>(value)));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Vec2 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Vec2>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Vec3 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Vec3>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Vec4 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Vec4>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Uvec2 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Uvec2>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Uvec3 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Uvec3>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Uvec4 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Uvec4>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Ivec2 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Ivec2>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Ivec3 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Ivec3>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Ivec4 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Ivec4>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Matrix4x4 &value) -> Error {
  return shader->Send(context, key,
                      SpanGenericType<Math::Matrix4x4>::GetSpan(value));
};

auto UniformWriter::Send(const Ref<ShaderModule> &shader,
                         const GraphicsContext &context, const ResourceKey &key,
                         const Math::Matrix3x3 &value) -> Error {
  // Matrix4x3 since uniforms are std140 aligned
  static std::array<float, 12> matrix3x3Data; // NOLINT

#pragma unroll
  for (int row = 0; row < Math::Matrix3x3::Rows; row++) {
#pragma unroll
    for (int col = 0; col < Math::Matrix3x3::Cols; col++) {
      // NOLINTNEXTLINE, clangd thinks row, col are swapped, but that's intentional
      matrix3x3Data.at((row * 4) + col) = value.At(row, col);
    }
  }

  return shader->Send(
      context, key,
      SpanGenericType<std::array<float, 12>>::GetSpan(matrix3x3Data)); // NOLINT
};
} // namespace Graphics::Shader