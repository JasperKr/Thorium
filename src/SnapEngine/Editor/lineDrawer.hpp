#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Scene/camera.hpp"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

const std::vector<Graphics::VertexComponent> LineVertexComponents = {
    Graphics::VertexComponent{
        .name = "Position",
        .format = VK_FORMAT_R32G32_SFLOAT,
    },
    Graphics::VertexComponent{
        .name = "Color",
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    },
    Graphics::VertexComponent{
        .name = "Thickness",
        .format = VK_FORMAT_R32_SFLOAT,
    },
};

const Graphics::VertexFormat LineVertexFormat{LineVertexComponents, {6}};

struct LineDrawer {
  Ref<Graphics::Mesh> Mesh;
  Ref<Graphics::Shader> Shader;

  struct LineData {
    Math::Vec3 Start;
    Math::Vec3 End;
    Math::Vec4 Color;
    float Thickness;
  };

  struct LineVertex {
    Math::Vec3 Start;
    Math::Vec3 End;
    uint32_t Color;
    float Thickness;
  };

  static constexpr size_t MaxLineCount = 1e5;
  static constexpr size_t MaxVertexCount = MaxLineCount * 6;

  std::vector<LineData> Lines;

  void DrawLine(const Math::Vec3 &start, const Math::Vec3 &end,
                const Math::Vec4 &color, float thickness);

  auto Initialize(const Graphics::GraphicsContext &context) -> Error;
  auto DeInitialize() -> Error;

  auto GenerateMesh(const Graphics::GraphicsContext &context) -> Error;
  auto Render(const Graphics::GraphicsContext &context, Camera &camera)
      -> Error;
};

} // namespace Engine::Renderer