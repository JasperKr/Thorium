#include "lineDrawer.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/error.hpp"
#include "Scene/camera.hpp"

namespace Engine::Renderer {

void LineDrawer::DrawLine(const Math::Vec3 &start, const Math::Vec3 &end,
                          const Math::Vec4 &color, float thickness) {
  Lines.emplace_back(LineData{
      .Start = start, .End = end, .Color = color, .Thickness = thickness});
}

auto LineDrawer::Initialize(const Graphics::GraphicsContext &context) -> Error {
  Mesh = CHECK_RES(Graphics::Mesh::Create(context, LineVertexFormat,
                                          MaxVertexCount, "Lines mesh"));

  return {};
}

auto LineDrawer::DeInitialize() -> Error {
  Mesh = nullptr;
  return {};
}

auto LineDrawer::GenerateMesh(const Graphics::GraphicsContext &context)
    -> Error {
  std::vector<LineVertex> vertices;
  vertices.reserve(Lines.size()); // NOLINT

  for (const auto &line : Lines) {
    const auto &start = line.Start;
    const auto &end = line.End;
    const auto &color = line.Color;
    const auto &thickness = line.Thickness;

    // Calculate the direction and perpendicular vector

    // NOLINTBEGIN
    uint32_t packedColor = static_cast<uint32_t>(color.x) << 24 |
                           static_cast<uint32_t>(color.y) << 16 |
                           static_cast<uint32_t>(color.z) << 8 |
                           static_cast<uint32_t>(color.w);
    // NOLINTEND

    vertices.emplace_back(LineVertex{
        .Start = start,
        .End = end,
        .Color = packedColor,
        .Thickness = thickness,
    });
  }

  // NOLINTNEXTLINE
  auto span = std::span<uint8_t>(reinterpret_cast<uint8_t *>(vertices.data()),
                                 vertices.size() * sizeof(LineVertex));

  CHECK_ERR(Mesh->SetVertices(context, 0, span));

  return {};
}

auto LineDrawer::Render(const Graphics::GraphicsContext &context,
                        Camera &camera) -> Error {
  if (Lines.empty()) {
    return {};
  }

  CHECK_ERR(GenerateMesh(context));
  Lines.clear();

  return {};
}

} // namespace Engine::Renderer