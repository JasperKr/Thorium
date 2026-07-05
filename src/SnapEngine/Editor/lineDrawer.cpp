#include "lineDrawer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/error.hpp"
#include "Scene/camera.hpp"
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

void LineDrawer::DrawLine(const Math::Vec3 &start, const Math::Vec3 &end,
                          const Math::Vec4 &color, float thickness) {
  Lines.emplace_back(LineData{
      .Start = start, .End = end, .Color = color, .Thickness = thickness});
}

void LineDrawer::OverlayLine(const Math::Vec3 &start, const Math::Vec3 &end,
                             const Math::Vec4 &color, float thickness) {
  OverlayLines.emplace_back(LineData{
      .Start = start, .End = end, .Color = color, .Thickness = thickness});
}

auto LineDrawer::Initialize(const Graphics::GraphicsContext &context) -> Error {
  Mesh = CHECK_RES(Graphics::Mesh::Create(context, LineVertexFormat,
                                          MaxVertexCount, "Lines mesh"));

  Shader = CHECK_RES(
      Graphics::Shader::Create(context, "GUI/lineDrawer", "Line shader"));

  return {};
}

auto LineDrawer::Deinitialize() -> void {
  Mesh = nullptr;
  Shader = nullptr;
}

auto LineDrawer::GenerateMesh(const Graphics::GraphicsContext &context,
                              const std::vector<LineData> &lines) -> Error {
  std::vector<LineVertex> vertices;
  vertices.reserve(lines.size()); // NOLINT

  for (const auto &line : lines) {
    const auto &start = line.Start;
    const auto &end = line.End;
    const auto &color = line.Color;
    const auto &thickness = line.Thickness;

    // Calculate the direction and perpendicular vector

    // NOLINTBEGIN
    uint32_t packedColor = static_cast<uint32_t>(color.x * 255.0F) |
                           static_cast<uint32_t>(color.y * 255.0F) << 8 |
                           static_cast<uint32_t>(color.z * 255.0F) << 16 |
                           static_cast<uint32_t>(color.w * 255.0F) << 24;
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

  if (Lines.empty() && OverlayLines.empty()) {
    return {};
  }

  Graphics::DynamicRendering::SetShader(Shader);
  Graphics::DynamicRendering::SetDepthMode(true, false, VK_COMPARE_OP_GREATER);

  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};
  CHECK_ERR(Shader->Send(cameraBufferKey, camera.GetBuffer()));

  static auto viewportSizeKey =
      Graphics::ResourceKey{"PushConstants", "viewportSize"};

  CHECK_ERR(Graphics::UniformWriter::Send(Shader, viewportSizeKey,
                                          camera.GetDimensions()));

  Mesh->SetDrawRange({.Offset = 0, .Count = 6}); // NOLINT
  if (!Lines.empty()) {
    CHECK_ERR(GenerateMesh(context, Lines));

    CHECK_ERR(Graphics::Draw(context, *Mesh, Lines.size()));
    Lines.clear();
  }

  if (OverlayLines.empty()) {
    return {};
  }

  CHECK_ERR(GenerateMesh(context, OverlayLines));

  Graphics::DynamicRendering::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);
  CHECK_ERR(Graphics::Draw(context, *Mesh, OverlayLines.size()));
  OverlayLines.clear();

  return {};
}

} // namespace Engine::Renderer